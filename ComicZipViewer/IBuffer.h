#pragma once
#include "framework.h"

class IBuffer
{
public:
	virtual ~IBuffer() = default;
	virtual bool CanMapped() = 0;
	virtual uint32_t GetLength() = 0;
	virtual void* GetPointer() = 0;
	virtual uint32_t Copy(void* dst, uint32_t offset, uint32_t length) const = 0;
};

class BufferStream: public IStream
{
public:
	BufferStream(IBuffer* pBuffer);
	~BufferStream();
	HRESULT QueryInterface(const IID& riid, void** ppvObject) override;
	ULONG AddRef() override;
	ULONG Release() override;
	HRESULT Read(void* pv, ULONG cb, ULONG* pcbRead) override;
	HRESULT Write(const void* pv, ULONG cb, ULONG* pcbWritten) override;
	HRESULT Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) override;
	HRESULT SetSize(ULARGE_INTEGER libNewSize) override;
	HRESULT CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) override;
	HRESULT Commit(DWORD grfCommitFlags) override;
	HRESULT Revert() override;
	HRESULT LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override;
	HRESULT UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) override;
	HRESULT Stat(STATSTG* pstatstg, DWORD grfStatFlag) override;
	HRESULT Clone(IStream** ppstm) override;

private:
	std::atomic_ulong m_refCount;
	IBuffer* m_pBuffer;
	size_t m_offset;
};