#include "framework.h"
#include "SimpleBuffer.h"

SimpleBuffer::SimpleBuffer(uint32_t size)
{
	m_mem.resize(size);
}

SimpleBuffer::~SimpleBuffer()
{
}

bool SimpleBuffer::CanMapped()
{
	return true;
}

uint32_t SimpleBuffer::GetLength()
{
	return m_mem.size();
}

void* SimpleBuffer::GetPointer()
{
	return static_cast<void*>(m_mem.data());
}

uint32_t SimpleBuffer::Copy(void* dst, uint32_t offset, uint32_t length) const
{
	if(offset >= m_mem.size())
		return 0;

	uint32_t end = std::min(static_cast<uint32_t>(m_mem.size()), offset + length);
	uint32_t readByteCount = end - offset;
	if(readByteCount == 0)
		return 0;

	memcpy(dst, (void*)(m_mem.data() + offset), readByteCount);
	return readByteCount;
}
