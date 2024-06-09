#include "framework.h"
#include <wx/stream.h>
#include "PageCollectionZip.h"
#include "SimpleBuffer.h"

struct Entry
{
	uintptr_t ptr;
	size_t size;
	Entry *prev;
	Entry *next;
};

template<typename _T>
class Lru
{
public:

	Lru()
	{
		dummy_head.prev = nullptr;
		dummy_head.next = &dummy_tail;
		dummy_tail.prev = &dummy_head;
		dummy_tail.next = nullptr;
	}

	// 가장 마지막에 사용한 엔트리를 방출한다.
	_T *EmitEntry()
	{
		_T *head = dummy_head.next;
		if(head == &dummy_tail)
		{
			return nullptr;
		}

		_T *new_head = head->next;
		dummy_head.next = new_head;
		new_head->prev = &dummy_head;
		head->next = nullptr;
		head->prev = nullptr;
		return head;
	}

	// 엔트리의 순서를 제일 마지막에 놓는다.
	void AbsorbEntry(_T *const entry)
	{
		if(entry->prev != nullptr)
		{
			_T *const prev = entry->prev;
			_T *const next = entry->next;
			prev->next = next;
			next->prev = prev;
		}

		_T *const old_tail = dummy_tail.prev;
		dummy_tail.prev = entry;
		entry->prev = old_tail;
		entry->next = &dummy_tail;
		old_tail->next = entry;
	}

	void RemoveEntry(_T *const entry)
	{
		if(entry->next == nullptr)
			return;

		_T *const prev = entry->prev;
		_T *const next = entry->next;
		prev->next = next;
		next->prev = prev;
		entry->prev = nullptr;
		entry->next = nullptr;
	}

private:
	_T dummy_head;
	_T dummy_tail;
};

template<size_t PAGE_SIZE>
constexpr size_t GetExponent()
{
	static_assert(PAGE_SIZE % 2 == 0);
	int exponent = 0;
	size_t page_size = PAGE_SIZE;
	while(page_size != 1)
	{
		page_size = page_size / 2;
		exponent++;
	}
	return exponent; // num이 2의 거듭제곱이므로 지수 반환
}

template<size_t NUM_ENTRY, size_t _PAGE_SIZE>
struct EntryTable
{
	static constexpr size_t PAGE_SIZE = _PAGE_SIZE;
	struct Index
	{
		static constexpr size_t EXPONENT = GetExponent<PAGE_SIZE>();
		size_t index;
		explicit operator size_t() const { return index; }
		template<typename U>
		constexpr static Index FromFileOffset(U offset)
		{
			return Index{(size_t)offset >> EXPONENT};
		}

		template <typename U>
		constexpr size_t ConvertToPageOffset(U fileOffset)
		{
			return (size_t)fileOffset - ToFileOffset();
		}

		constexpr size_t ToFileOffset() const
		{ 
			return index << EXPONENT; 
		}

		Index operator+(int d) const
		{ 
			Index ret{*this};
			return ret += d;
		}

		Index& operator+=(int d)
		{
			index += d;
			return *this;
		}

		Index& operator-=(int d)
		{
			index -= d;
			return *this;
		}

		Index operator-(int d) const
		{
			Index ret{*this};
			return ret -= d;
		}

		ssize_t operator-(Index rhs) const { return index - rhs.index; }
		bool operator==(Index rhs) const { return rhs.index == index; }
		bool operator!=(Index rhs) const { return !(rhs == *this); }
		bool operator<(Index rhs) const { return index < rhs.index; }
		bool operator>(Index rhs) const { return index > rhs.index; }

	};

	Entry &operator[](Index index) { return entries[(size_t)index]; }
	Entry entries[NUM_ENTRY];

};

template<typename T, size_t MAX_SIZE>
class LimitedSizedList
{
	struct Node
	{
		uint8_t payload[sizeof(T)];
		Node *prev;
		Node *next;
	};
public:
	template<typename value_type>
	struct It
	{
		Node* node;
		It operator++(int) const
		{
			It ret{ *this };
			return ++ret;
		}

		It& operator++()
		{
			node = node->next;
			return *this;
		}

		It operator--(int) const
		{
			It ret{ *this };
			return --ret;
		}

		It& operator--()
		{
			node = node->prev;
			return *this;
		}

		value_type& operator*() const
		{
			return *(value_type*)node->payload;
		}

		value_type* operator->() const
		{
			return (value_type*)node->payload;
		}

		bool operator==(It rhs) const
		{
			return node == rhs.node;
		}

		bool operator!=(It rhs) const
		{
			return node != rhs.node;
		}
	};

	LimitedSizedList() : nodes{}
	{ 
		size_ = 0;
		idle_head = nullptr;
		dummy_head.next = &dummy_tail;
		dummy_head.prev = nullptr;
		dummy_tail.prev = &dummy_head;
		dummy_tail.next = nullptr;
		for ( Node& node: nodes )
		{
			node.next = idle_head;
			idle_head = &node;
		}
	}

	LimitedSizedList(const LimitedSizedList& rhs)
	: LimitedSizedList()
	{
		for(auto& it: rhs)
		{
			emplace_back(it);
		}
	}

	LimitedSizedList(LimitedSizedList&& rhs) noexcept
		: LimitedSizedList()
	{
		while(rhs.size() != 0)
		{
			emplace_back(std::move(rhs.front()));
			rhs.pop_front();
		}
	}

	~LimitedSizedList()
	{
		while (size() != 0)
		{
			pop_front();
		}
	}

	LimitedSizedList& operator=(const LimitedSizedList& rhs)
	{
		if (this != &rhs)
		{
			~LimitedSizedList();
			new(this) LimitedSizedList(rhs);
		}

		return *this;
	}

	template<class ...Args>
	void emplace_back(Args &&...args)
	{
		emplace(end(), std::forward<Args>(args)...);
	}

	void pop_front()
	{
		if (size() == 0)
		{
			return;
		}


		erase(begin());
	}

	void pop_back()
	{
		if (size() == 0)
			return;

		erase(--end());
	}

	T& front()
	{
		if (size() == 0)
		{
			return *(T *)nullptr;
		}

		return *(T*)dummy_head.next->payload;
	}

	T& back()
	{
		if (size() == 0)
			return *(T*)nullptr;

		return *(T*)dummy_tail.prev->payload;
	}

	size_t size() const
	{ 
		return size_;
	}

	It<T> begin() { return It<T>{ dummy_head.next }; }
	It<T> end() { return It<T>{ &dummy_tail }; }

	It<const T> begin() const { return It<const T>{ dummy_head.next }; }
	It<const T> end() const { return It<const T>{ &dummy_tail }; }

	It<T> erase(It<T> it)
	{
		if(nodes <= it.node && it.node <= (nodes + MAX_SIZE))
		{
			It<T> ret = it;
			++ret;
			Node* node = it.node;
			Node* prev = node->prev;
			Node* next = node->next;
			prev->next = next;
			next->prev = prev;
			reinterpret_cast<T*>(node->payload)->~T();
			node->prev = nullptr;
			node->next = idle_head;
			idle_head = node;
			size_ -= 1;
			return ret;
		}
		
		return end();
	}

	template<class ...Args>
	void emplace(It<T> it, Args &&...args)
	{
		if (size() == MAX_SIZE)
			throw std::bad_alloc();

		Node* node = idle_head;
		idle_head = idle_head->next;
		node->prev = nullptr;
		node->next = nullptr;

		new(node->payload) T(std::forward<Args>(args)...);
		Node* next = it.node;
		Node* prev = next->prev;
		prev->next = node;
		next->prev = node;
		node->prev = prev;
		node->next = next;
		size_ += 1;
	}

private:
	Node nodes[MAX_SIZE];
	size_t size_;
	Node *idle_head;
	Node dummy_head;
	Node dummy_tail;
};

class WinFileInputStrm : public wxInputStream
{
	static constexpr size_t MAX_PAGE_COUNT = 64;
	static constexpr size_t PAGE_SIZE = 1024 * 1024 * 4;
	using EntryTable = EntryTable<1024, PAGE_SIZE>;
	using Index = EntryTable::Index;
	struct OverlappedEx: OVERLAPPED
	{
		WinFileInputStrm *stream;
	};

	static void CALLBACK OnIoCompleted(_In_ DWORD dwErrorCode , _In_ DWORD dwNumberOfBytesTransfered , _Inout_ LPOVERLAPPED lpOverlapped)
	{
		OverlappedEx *overapped = (OverlappedEx *)lpOverlapped;
		overapped->stream->IoCompeleted(dwErrorCode, dwNumberOfBytesTransfered, overapped);
	}

	void IoCompeleted(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, OverlappedEx *overlapped)
	{
		if(current_waited_overlapped == overlapped)
			SetEvent(hEvent);

		Index index = Index::FromFileOffset(overlapped->Offset);
		Entry &entry = table[index];
		
		if( dwErrorCode == ERROR_OPERATION_ABORTED)
		{
			// Cancel 당했다
			void *ptr = overlapped->hEvent;
			overlapped->hEvent = nullptr;
			idle_memory.emplace_back(ptr);
			idle_overlapped.emplace_back(overlapped);
			lru.RemoveEntry(&entry);
			SetEvent(hEvent);
			return;
		}

		if ( dwErrorCode != ERROR_SUCCESS )
		{
			// 에러로 파일 IO 실패
			void *ptr = overlapped->hEvent;
			overlapped->hEvent = nullptr;
			idle_memory.emplace_back(ptr);
			idle_overlapped.emplace_back(overlapped);
			lru.RemoveEntry(&entry);
			SetEvent(hEvent);
			return;
		}
		

		if(dwNumberOfBytesTransfered == 0)
		{
			// 파일을 못 읽었다.
			entry.ptr = 0;
			void *ptr = overlapped->hEvent;
			overlapped->hEvent = nullptr;
			idle_memory.emplace_back(ptr);
			idle_overlapped.emplace_back(overlapped);
			lru.RemoveEntry(&entry);
			SetEvent(hEvent);
			return;
		}

		if ( entry.ptr == 0 )
		{
			// 완료는 되었지만 LRU에 따라서 방출되었다.
			void *ptr = overlapped->hEvent;
			overlapped->hEvent = nullptr;
			idle_memory.emplace_back(ptr);
			idle_overlapped.emplace_back(overlapped);
			lru.RemoveEntry(&entry);
			SetEvent(hEvent);
			return;
		}

		entry.ptr = (uintptr_t)overlapped->hEvent;
		entry.size = dwNumberOfBytesTransfered;
		idle_overlapped.emplace_back(overlapped);
	}

public:
	WinFileInputStrm(const wxString& fileName) : overlappeds{}, hEvent{}, offset{}, table{}, current_waited_overlapped{}
	{
		fileSize = 0;
		big_page = malloc(MAX_PAGE_COUNT * PAGE_SIZE);
		uintptr_t base = (uintptr_t)big_page;
		for ( size_t i = 0; i < MAX_PAGE_COUNT; ++i )
		{
			idle_memory.emplace_back((void*)(base + PAGE_SIZE * i));
		}

		for(auto &o: overlappeds)
		{
			o.stream = this;
			idle_overlapped.emplace_back(&o);
		}

		hEvent = CreateEventW(nullptr, false, false, nullptr);
		const wchar_t *filePath = fileName.wc_str();
		hFile = CreateFileW(filePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
							FILE_ATTRIBUTE_READONLY | FILE_FLAG_OVERLAPPED | FILE_FLAG_RANDOM_ACCESS, nullptr);
		if(hFile != INVALID_HANDLE_VALUE)
		{
			LARGE_INTEGER largeFileSize;
			GetFileSizeEx(hFile, &largeFileSize);
			fileSize = largeFileSize.QuadPart;
			BeginReadAsync(Index{0});
		}
	}

	~WinFileInputStrm() override
	{
		CancelIo(hFile);
		while(idle_overlapped.size() != MAX_PAGE_COUNT)
		{
			WaitForSingleObjectEx(hEvent, 0, true);
		}

		CloseHandle(hFile);
		free(big_page);
	}

	bool IsOk() const override { return hFile != INVALID_HANDLE_VALUE; }
	
	size_t GetSize() const override { return fileSize; }

	// returns true if the streams supports seeking to arbitrary offsets
	bool IsSeekable() const override { return true; }

protected:
	wxFileOffset OnSysTell() const override { return static_cast<wxFileOffset>(offset); }

	size_t OnSysRead(void *buffer, size_t size) override
	{
		Index index = Index::FromFileOffset(offset);
		Entry &entry = table[index];
		// 엔트리에 할당된 메모리가 없다면 읽은 것이 없다.
		// 읽기 작업이 수행되어야 한다.
		BeginReadAsync(index);
		
		lru.AbsorbEntry(&entry);
		if(entry.ptr <= MAX_PAGE_COUNT)
		{
			// 엔트리가 비동기 요청을 했다.
			// 해당 처리가 완료될 때까지 기다린다.
			OverlappedEx *overlapped = &overlappeds[entry.ptr - 1];
			current_waited_overlapped = overlapped;
			do
			{
				WaitForSingleObjectEx(hEvent, INFINITE, true);
				if ( overlapped->hEvent == nullptr )
				{
					// 어떠한 이유로 IO가 실패했다. Retry 한다.
					BeginReadAsync(index);
					if(entry.ptr == 0)
						return wxInvalidOffset;

					overlapped = &overlappeds[entry.ptr - 1];
				}
			}
			while(entry.ptr <= MAX_PAGE_COUNT);

			current_waited_overlapped = nullptr; 
			if(entry.ptr == 0)
				return 0;
		}
		
		// 엔트리에서
		size_t pageOffset = index.ConvertToPageOffset(offset);
		size_t readSize = std::min(pageOffset + size, entry.size) - pageOffset;
		uintptr_t ptr = entry.ptr + pageOffset;
		memcpy(buffer, (void *)ptr, readSize);
		offset += readSize;
		if(size > PAGE_SIZE)
		{
			// 만약에 size가 PAGE_SIZE보다 크다면 선제적으로 비동기 읽기 요청을 내린다.
			Index destination = Index::FromFileOffset(offset + PAGE_SIZE);
			for(Index i = index + 1; i == destination && (i - index) < MAX_PAGE_COUNT / 2; i += 1)
			{
				BeginReadAsync(i);
			}
		}

		return readSize;
	}

	wxFileOffset OnSysSeek(wxFileOffset pos, wxSeekMode mode) override
	{
		wxFileOffset new_offset = offset;
		switch ( mode )
		{
		case wxFromCurrent:
			new_offset += pos;
			break;

		case wxFromStart:
			new_offset = pos;
			break;

		case wxFromEnd:
			new_offset = fileSize + pos;
			break;
		}

		if(new_offset >= fileSize)
			return wxInvalidOffset;

		Index index = Index::FromFileOffset(new_offset);
		
		BeginReadAsync(index);
		if ( new_offset > offset )
		{
			BeginReadAsync(index + 1);
			BeginReadAsync(index + 2);
		}
		else if( new_offset < offset)
		{
			BeginReadAsync(index - 1);
			BeginReadAsync(index - 2);
		}

		offset = new_offset;
		
		return new_offset;
	}

private:

	BOOL BeginReadAsync(Index index)
	{
		if(index > Index::FromFileOffset(fileSize))
			return false;

		Entry &entry = table[index];
		if(entry.ptr != 0)
		{
			lru.AbsorbEntry(&entry);
			return true;
		}

		if ( idle_memory.size() == 0 )
		{
			Entry* emitted_entry = lru.EmitEntry();
			if(emitted_entry->ptr <= MAX_PAGE_COUNT)
			{
				auto overlapped = &overlappeds[emitted_entry->ptr - 1];
				entry.ptr = 0;
				// IO 를 캔슬시킨다
				CancelIoEx(hFile, overlapped);
				current_waited_overlapped = overlapped;
				while(idle_memory.size() != 0)
				{
					WaitForSingleObjectEx(hEvent, 15, true);
				}

				current_waited_overlapped = nullptr;
			}

			if(emitted_entry->ptr != 0)
			{
				idle_memory.emplace_back((void *)emitted_entry->ptr);
			}

			emitted_entry->ptr = 0;
		}
		
		void *mem = idle_memory.front();
		OverlappedEx *overlapped = idle_overlapped.front();
		idle_memory.pop_front();
		idle_overlapped.pop_front();
		overlapped->hEvent = mem;
		overlapped->Offset = index.ToFileOffset();
		entry.ptr = (overlapped - overlappeds) + 1;
		if (ReadFileEx(hFile , mem , PAGE_SIZE , overlapped , OnIoCompleted) )
		{
			lru.AbsorbEntry(&entry);
			return true;
		}

		DWORD err = ::GetLastError();
		if(err == ERROR_IO_PENDING)
		{
			lru.AbsorbEntry(&entry);
			return true;
		}

		// 실패했으면 메모리를 회수한다.
		entry.ptr = 0;
		lru.RemoveEntry(&entry);
		idle_memory.emplace_back(mem);
		idle_overlapped.emplace_back(overlapped);
		return false;
	}

	bool ReadSync(Index index)
	{
		BeginReadAsync(index);
		Entry &entry = table[index];
		if(entry.ptr == 0)
			return false;

		current_waited_overlapped = &overlappeds[entry.ptr - 1];
		while(WaitForSingleObjectEx(hEvent, INFINITE, true) != WAIT_OBJECT_0)
		{
		}

		current_waited_overlapped = nullptr;
		return entry.ptr > MAX_PAGE_COUNT;
	}

private:
	HANDLE hFile;
	HANDLE hEvent;
	wxFileOffset fileSize;
	wxFileOffset offset;
	EntryTable table;
	OverlappedEx overlappeds[MAX_PAGE_COUNT];
	OverlappedEx* current_waited_overlapped;
	void *big_page;
	LimitedSizedList<OverlappedEx *, MAX_PAGE_COUNT> idle_overlapped;
	LimitedSizedList<void *, MAX_PAGE_COUNT> idle_memory;
	Lru<Entry> lru;
};

PageCollectionZip::PageCollectionZip(const wxString& filePath)
	: m_pFileStream(nullptr)
	, m_pZipStream(nullptr)
{
	m_pFileStream = new WinFileInputStrm(filePath);
	if(!m_pFileStream->IsOk())
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
		entry.second->UnRef();
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
