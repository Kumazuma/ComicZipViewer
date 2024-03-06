#include "framework.h"
#include <wx/stream.h>
#include "PageCollectionZip.h"

#include <deque>

#include "SimpleBuffer.h"
#include <map>

class WinAsyncInputStreamMemoryManager
{
public:
	static constexpr size_t IO_BUF_SIZE = (UINT16_MAX + 1) << 6; // 4 MiB
	WinAsyncInputStreamMemoryManager()
	{
		
	}

	~WinAsyncInputStreamMemoryManager()
	{
		for(uint8_t* ptr: reservedPages_)
		{
			VirtualFree(ptr, 0, MEM_RESERVE);
		}
	}

	static std::shared_ptr<WinAsyncInputStreamMemoryManager> GetInstance()
	{
		return s_instance;
	}

	uint8_t* LoanPage()
	{
		uint8_t* ptr; 
		if(reservedPages_.empty())
		{
			ptr = static_cast<uint8_t*>(VirtualAlloc(nullptr, IO_BUF_SIZE, MEM_RESERVE, PAGE_READWRITE));
		}
		else
		{
			ptr = reservedPages_.front();
			reservedPages_.pop_front();
		}

		VirtualAlloc(ptr, IO_BUF_SIZE, MEM_COMMIT, PAGE_READWRITE);
		return ptr;
	}

	void ReturnLoanPage(uint8_t* ptr)
	{
		VirtualFree(ptr, IO_BUF_SIZE, MEM_DECOMMIT);
		reservedPages_.push_back(ptr);
	}

private:
	std::deque<uint8_t*> reservedPages_;

	static std::shared_ptr<WinAsyncInputStreamMemoryManager> s_instance;
};

std::shared_ptr<WinAsyncInputStreamMemoryManager> WinAsyncInputStreamMemoryManager::s_instance = std::make_shared<WinAsyncInputStreamMemoryManager>();

class WinAsyncInputStream : public wxInputStream
{
	static constexpr size_t IO_BUF_SIZE = WinAsyncInputStreamMemoryManager::IO_BUF_SIZE;
	struct OverlappedEx : OVERLAPPED
	{
		WinAsyncInputStream* self;
		DWORD Length;
		uint8_t *page;
	};
	
	static constexpr uintptr_t OffsetToTableKey(uintptr_t offset) { return offset & (~(IO_BUF_SIZE - 1)); }
	static void CALLBACK ProcessIoComplete(_In_ DWORD dwErrorCode, _In_ DWORD dwNumberOfBytesTransfered,
										   _Inout_ LPOVERLAPPED lpOverlapped)
	{
		OverlappedEx *const job = (OverlappedEx *)lpOverlapped;
		WinAsyncInputStream *const self = job->self;
		std::unique_ptr<OverlappedEx> ptr;
		auto it = self->workingJobs_.find(job->Offset);
		if(it != self->workingJobs_.end())
		{
			ptr = std::move(it->second);
			self->workingJobs_.erase(it);
		}

		if(dwErrorCode == 0)
		{
			job->Length = dwNumberOfBytesTransfered;
			if(self->tableLoadedFileChunk_.count(OffsetToTableKey(job->Offset)) == 0)
			{
				self->tableLoadedFileChunk_.emplace(OffsetToTableKey(job->Offset), std::move(ptr));
			}
			else
			{
				self->ReturnLoan(std::move(ptr));
			}

			if(self->tableLoadedFileChunk_.size() < 16 && dwNumberOfBytesTransfered == IO_BUF_SIZE &&
			   job->Offset == OffsetToTableKey(self->offset_))
			{
				if(self->tableLoadedFileChunk_.count(job->Offset + IO_BUF_SIZE) == 0)
				{
					auto newTask = self->LoanJob();
					LARGE_INTEGER largeInt;
					largeInt.LowPart = job->Offset;
					largeInt.HighPart = job->OffsetHigh;
					largeInt.QuadPart += IO_BUF_SIZE;
					newTask->Offset = largeInt.LowPart;
					newTask->OffsetHigh = largeInt.HighPart;
					newTask->Length = IO_BUF_SIZE;
					self->ReadFile(std::move(newTask));
				}
			}
		}
		else
		{
			self->recycleJobs_.push_back(std::move(ptr));
		}
	}

	void ReadFile(std::unique_ptr<OverlappedEx> &&job)
	{
		uintptr_t offset = job->Offset;
		if(workingJobs_.count(offset) == 0 && tableLoadedFileChunk_.count(offset) == 0)
		{
			job->self = this;
			ReadFileEx(hFile_, job->page, job->Length, job.get(), &ProcessIoComplete);
			workingJobs_.emplace(offset, std::move(job));
		}
		else
		{
			ReturnLoan(std::move(job));
		}
	}

public:
	WinAsyncInputStream() : hFile_(INVALID_HANDLE_VALUE), hEvent_(), offset_(0), fileSize_(0) {}
	WinAsyncInputStream(WinAsyncInputStream&&) = delete;
	WinAsyncInputStream(const WinAsyncInputStream&) = delete;

	bool Open(const wxString &filePath)
	{
		if(hFile_ != INVALID_HANDLE_VALUE)
			return false;

		HANDLE hFile = CreateFileW(filePath.wc_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
								   FILE_ATTRIBUTE_READONLY | FILE_FLAG_OVERLAPPED | FILE_FLAG_RANDOM_ACCESS, nullptr);
		if(hFile == INVALID_HANDLE_VALUE)
			return false;

		hFile_ = hFile;
		LARGE_INTEGER fileSize;
		GetFileSizeEx(hFile_, &fileSize);
		fileSize_ = fileSize.QuadPart;
		hEvent_ = CreateEventW(nullptr, true, false, nullptr);
		auto firstPayload = LoanJob();
		firstPayload->Length = IO_BUF_SIZE;
		ReadFile(std::move(firstPayload));
		return true;
	}

	~WinAsyncInputStream() override
	{
		if(hFile_ == INVALID_HANDLE_VALUE)
			return;

		auto instance = WinAsyncInputStreamMemoryManager::GetInstance();
		CancelIo(hFile_);
		while(!workingJobs_.empty())
		{
			WaitForSingleObjectEx(hEvent_, 15, true);
		}

		CloseHandle(hFile_);
		CloseHandle(hEvent_);
		hFile_ = INVALID_HANDLE_VALUE;
		for(auto& it: tableLoadedFileChunk_)
		{
			instance->ReturnLoanPage(it.second->page);
			it.second->page = nullptr;
		}
		
		for(auto& it: workingJobs_)
		{
			instance->ReturnLoanPage(it.second->page);
			it.second->page = nullptr;
		}

		for(auto& it: recycleJobs_)
		{
			instance->ReturnLoanPage(it->page);
			it->page = nullptr;
		}
	}

	bool IsSeekable() const override { return true; }

protected:
	// returns wxInvalidOffset on error
	wxFileOffset OnSysSeek(wxFileOffset pos, wxSeekMode mode) override
	{
		size_t newOffset;
		switch(mode)
		{
		case wxFromCurrent:
			newOffset = offset_ + pos;
			break;

		case wxFromEnd:
			newOffset = fileSize_ + pos;
			break;

		case wxFromStart:
			newOffset = pos;
			break;

		default:
			return wxInvalidOffset;
		}


		if(newOffset != offset_)
		{
			{
				auto it = tableLoadedFileChunk_.begin();
				while(it != tableLoadedFileChunk_.end())
				{
					if(newOffset <= it->first)
					{
						++it;
						continue;
					}

					ReturnLoan(std::move(it->second));
					it = tableLoadedFileChunk_.erase(it);
				}
			}

			do
			{
				if(tableLoadedFileChunk_.count(OffsetToTableKey(newOffset)) != 0)
					break;

				if(workingJobs_.count(OffsetToTableKey(newOffset)) != 0)
					break;

				uintptr_t pageOffset = OffsetToTableKey(newOffset);
				while(workingJobs_.size() < 4)
				{
					if(pageOffset > fileSize_)
						break;

					auto newTask = LoanJob();
					LARGE_INTEGER largeInt;
					largeInt.QuadPart = pageOffset;
					newTask->Offset = largeInt.LowPart;
					newTask->OffsetHigh = largeInt.HighPart;

					newTask->Length = IO_BUF_SIZE;
					pageOffset += IO_BUF_SIZE;
					ReadFile(std::move(newTask));
				}
			}
			while(false);
		}

		offset_ = newOffset;
		return static_cast<wxFileOffset>(offset_);
	}

	// return the current position of the stream pointer or wxInvalidOffset
	wxFileOffset OnSysTell() const override { return static_cast<wxFileOffset>(offset_); }

	size_t OnSysRead(void *buffer, size_t size) override
	{
		if(size == 0 || fileSize_ == offset_)
		{
			return 0;
		}

		auto it = tableLoadedFileChunk_.find(OffsetToTableKey(offset_));
		if(it == tableLoadedFileChunk_.end())
		{
			if(workingJobs_.count(OffsetToTableKey(offset_)) == 0)
			{
				uintptr_t pageOffset = OffsetToTableKey(offset_);
				for(int i = 0; i < 4; ++i)
				{
					if(pageOffset > fileSize_)
						break;

					if(tableLoadedFileChunk_.count(pageOffset) == 0)
					{
						auto newTask = LoanJob();
						LARGE_INTEGER largeInt;
						largeInt.QuadPart = pageOffset;
						newTask->Offset = largeInt.LowPart;
						newTask->OffsetHigh = largeInt.HighPart;

						newTask->Length = IO_BUF_SIZE;
						pageOffset += IO_BUF_SIZE;
						ReadFile(std::move(newTask));
					}
				}
			}

			while(!workingJobs_.empty())
			{
				WaitForSingleObjectEx(hEvent_, 100, true);
			}

			auto it2 = tableLoadedFileChunk_.begin();
			auto end2 = tableLoadedFileChunk_.find(OffsetToTableKey(offset_));
			while(it2 != end2)
			{
				ReturnLoan(std::move(it2->second));
				it2 = tableLoadedFileChunk_.erase(it2);
			}

			++it2;
			uintptr_t preOffset = OffsetToTableKey(offset_);
			while(it2 != tableLoadedFileChunk_.end())
			{
				if(it2->first == preOffset + IO_BUF_SIZE)
				{
					preOffset = it2->first;
					++it2;
					continue;
				}

				while(it2 != tableLoadedFileChunk_.end())
				{
					ReturnLoan(std::move(it2->second));
					it2 = tableLoadedFileChunk_.erase(it2);
				}

				break;
			}

			it = tableLoadedFileChunk_.find(OffsetToTableKey(offset_));
		}
		else
		{
			if (!workingJobs_.empty())
			{
				WaitForSingleObjectEx(hEvent_, 0, true);
			}

			uintptr_t pageOffset = OffsetToTableKey(offset_) + IO_BUF_SIZE;
			if (tableLoadedFileChunk_.count(pageOffset) == 0)
			{
				auto newTask = LoanJob();
				LARGE_INTEGER largeInt;
				largeInt.QuadPart = pageOffset;
				newTask->Offset = largeInt.LowPart;
				newTask->OffsetHigh = largeInt.HighPart;
				newTask->Length = IO_BUF_SIZE;
				ReadFile(std::move(newTask));
			}
		}

		auto& chunk = it->second;
		const uintptr_t offset = offset_ & (IO_BUF_SIZE - 1);
		const size_t readSize = std::min(chunk->Length - offset, size);
		memcpy(buffer, chunk->page + offset, readSize);
		offset_ += readSize;
		if(offset + readSize == chunk->Length && offset_ != fileSize_)
		{
			if(tableLoadedFileChunk_.count(offset_) == 0 && workingJobs_.count(offset_) == 0)
			{
				std::unique_ptr<OverlappedEx> c = std::move(chunk);
				LARGE_INTEGER largeInt;
				largeInt.QuadPart = offset_;
				c->Offset = largeInt.LowPart;
				c->OffsetHigh = largeInt.HighPart;
				c->Length = IO_BUF_SIZE;
				ReadFile(std::move(c));
				tableLoadedFileChunk_.erase(it);
			}
			else
			{
				ReturnLoan(std::move(chunk));
				tableLoadedFileChunk_.erase(it);
			}
		}

		return readSize;
	}

private:
	std::unique_ptr<OverlappedEx> LoanJob()
	{
		std::unique_ptr<OverlappedEx> job;
		if(recycleJobs_.empty())
		{
			auto instance = WinAsyncInputStreamMemoryManager::GetInstance();
			job = std::make_unique<OverlappedEx>();
			job->page = instance->LoanPage();
		}
		else
		{
			job = std::move(recycleJobs_.front());
			recycleJobs_.pop_front();
		}

		job->Length = 0;
		job->Offset = 0;
		job->hEvent = nullptr;
		job->self = this;
		return job;
	}

	void ReturnLoan(std::unique_ptr<OverlappedEx> &&job)
	{
		recycleJobs_.push_back(std::move(job));
	}

private:
	HANDLE hFile_;
	HANDLE hEvent_;
	size_t offset_;
	size_t fileSize_;
	std::map<uintptr_t, std::unique_ptr<OverlappedEx>> tableLoadedFileChunk_;
	std::map<uintptr_t, std::unique_ptr<OverlappedEx>> workingJobs_;
	std::list<std::unique_ptr<OverlappedEx>> recycleJobs_;
};

PageCollectionZip::PageCollectionZip(const wxString& filePath)
	: m_pFileStream(nullptr)
	, m_pZipStream(nullptr)
{
	WinAsyncInputStream* pFileStream = new WinAsyncInputStream();
	m_pFileStream = pFileStream;
	if(!pFileStream->Open(filePath))
		return;

	m_pZipStream =  new wxZipInputStream(*m_pFileStream);
	if(!m_pZipStream->IsOk())
		return;

	wxZipEntry* entry;
	while((entry = m_pZipStream->GetNextEntry()) != nullptr)
	{
		wxString name = entry->GetName();
		m_entryTable.emplace(name, entry);
	}

	m_pageCount = m_entryTable.size();
}

PageCollectionZip::~PageCollectionZip()
{
	delete m_pFileStream;
	delete m_pZipStream;
	m_pZipStream = nullptr;
	m_pFileStream = nullptr;
	for(auto& entry: m_entryTable)
	{
		delete entry.second;
	}
}

void PageCollectionZip::GetPageNames(std::vector<wxString>* names) const
{
	for(auto& pair: m_entryTable)
	{
		names->push_back(pair.first);
	}
}

IBuffer* PageCollectionZip::GetPage(const wxString& pageName)
{
	auto it = m_entryTable.find(pageName);
	if(it == m_entryTable.end())
		return nullptr;

	auto entry = it->second;
	if(!m_pZipStream->OpenEntry(*entry))
		return nullptr;

	auto* pSimpleBuffer = new SimpleBuffer(entry->GetSize());
	m_pZipStream->ReadAll(pSimpleBuffer ->GetPointer(), pSimpleBuffer ->GetLength());
	m_pZipStream->CloseEntry();

	return pSimpleBuffer;
}
