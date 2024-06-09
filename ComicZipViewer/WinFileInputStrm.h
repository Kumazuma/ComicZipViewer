#pragma once

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
		T* obj;
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
			return *(value_type*)node->obj;
		}

		value_type* operator->() const
		{
			return (value_type*)node->obj;
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
		static_assert(sizeof(payload[0]) == sizeof(T));
		size_ = 0;
		idle_head = nullptr;
		dummy_head.next = &dummy_tail;
		dummy_head.prev = nullptr;
		dummy_tail.prev = &dummy_head;
		dummy_tail.next = nullptr;
		for ( Node& node: nodes )
		{
			node.next = idle_head;
			node.obj = (T*)(payload[&node - nodes]);
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

		return *(T*)dummy_head.next->obj;
	}

	T& back()
	{
		if (size() == 0)
			return *(T*)nullptr;

		return *(T*)dummy_tail.prev->obj;
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
			reinterpret_cast<T*>(node->obj)->~T();
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

		new(node->obj) T(std::forward<Args>(args)...);
		Node* next = it.node;
		Node* prev = next->prev;
		prev->next = node;
		next->prev = node;
		node->prev = prev;
		node->next = next;
		size_ += 1;
	}

private:
	uint8_t payload[MAX_SIZE][sizeof(T)];
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

	static void CALLBACK OnIoCompleted(_In_ DWORD dwErrorCode , _In_ DWORD dwNumberOfBytesTransfered , _Inout_ LPOVERLAPPED lpOverlapped);

	void IoCompeleted(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, OverlappedEx *overlapped);

public:
	WinFileInputStrm(const wxString& fileName);

	~WinFileInputStrm() override;

	bool IsOk() const override { return hFile != INVALID_HANDLE_VALUE; }
	
	size_t GetSize() const override { return fileSize; }

	// returns true if the streams supports seeking to arbitrary offsets
	bool IsSeekable() const override { return true; }

protected:
	wxFileOffset OnSysTell() const override { return static_cast<wxFileOffset>(offset); }

	size_t OnSysRead(void *buffer, size_t size) override;

	wxFileOffset OnSysSeek(wxFileOffset pos, wxSeekMode mode) override;

private:

	BOOL BeginReadAsync(Index index);

	bool ReadSync(Index index);

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
