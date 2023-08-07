#pragma once
class IAllocator
{
public:
	virtual void* Alloc(size_t size) = 0;
	virtual void Free(void* ptr) = 0;
};
