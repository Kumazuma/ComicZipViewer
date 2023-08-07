#pragma once
class IBuffer
{
public:
	virtual ~IBuffer() = default;
	virtual bool CanMapped() = 0;
	virtual uint32_t GetLength() = 0;
	virtual void* GetPointer() = 0;
	virtual uint32_t Copy(void* dst, uint32_t offset, uint32_t length) const = 0;
};
