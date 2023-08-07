#pragma once
#include "IBuffer.h"
#include <vector>

class SimpleBuffer :
	public IBuffer
{
public:
	explicit SimpleBuffer(uint32_t size);
	~SimpleBuffer() override;
	bool CanMapped() override;
	uint32_t GetLength() override;
	void* GetPointer() override;
	uint32_t Copy(void* dst, uint32_t offset, uint32_t length) const override;

private:
	std::vector<uint8_t> m_mem;
};

