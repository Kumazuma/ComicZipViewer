#include "framework.h"
#include "WinFileInputStrm.h"

void WinFileInputStrm::OnIoCompleted(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered,
	LPOVERLAPPED lpOverlapped)
{
	OverlappedEx *overapped = (OverlappedEx *)lpOverlapped;
	overapped->stream->IoCompeleted(dwErrorCode, dwNumberOfBytesTransfered, overapped);
}

void WinFileInputStrm::IoCompeleted(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, OverlappedEx* overlapped)
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

WinFileInputStrm::WinFileInputStrm(const wxString& fileName): overlappeds{}, hEvent{}, offset{}, table{}, current_waited_overlapped{}
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

WinFileInputStrm::~WinFileInputStrm()
{
	CancelIo(hFile);
	while(idle_overlapped.size() != MAX_PAGE_COUNT)
	{
		WaitForSingleObjectEx(hEvent, 0, true);
	}

	CloseHandle(hFile);
	free(big_page);
}

size_t WinFileInputStrm::OnSysRead(void* buffer, size_t size)
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

wxFileOffset WinFileInputStrm::OnSysSeek(wxFileOffset pos, wxSeekMode mode)
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

BOOL WinFileInputStrm::BeginReadAsync(Index index)
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

bool WinFileInputStrm::ReadSync(Index index)
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
