#include "framework.h"
#include "IBuffer.h"

BufferStream::BufferStream(IBuffer* pBuffer)
	: m_refCount(1)
	, m_offset(0)
	, m_pBuffer(pBuffer) {

}

BufferStream::~BufferStream()
{
	delete m_pBuffer;
	m_pBuffer = nullptr;
}

HRESULT BufferStream::QueryInterface(const IID& riid, void** ppvObject)
{
	if(riid == __uuidof(IStream))
	{
		*reinterpret_cast<IStream**>(ppvObject) = this;
	}
	else if(riid == __uuidof(ISequentialStream))
	{
		*reinterpret_cast<ISequentialStream**>(ppvObject) = this;
	}
	else if(riid == __uuidof(IUnknown))
	{
		*reinterpret_cast<IUnknown**>(ppvObject) = this;
	}
	else
	{
		return E_NOINTERFACE;
	}

	this->AddRef();
	return S_OK;
}

ULONG BufferStream::AddRef()
{
	return m_refCount.fetch_add(1) + 1;
}

ULONG BufferStream::Release()
{
	ULONG refCount = m_refCount.fetch_sub(1);
	if(refCount == 1)
	{
		delete this;
	}

	return refCount - 1;
}

HRESULT BufferStream::Read(void* pv, ULONG cb, ULONG* pcbRead)
{
	size_t r = m_pBuffer->Copy(pv, m_offset, cb);
	m_offset += r;
	*pcbRead = r;
	if(r == 0)
		return E_BOUNDS;

	return S_OK;
}

HRESULT BufferStream::Write(const void* pv, ULONG cb, ULONG* pcbWritten)
{
	return E_NOTIMPL;
}

HRESULT BufferStream::Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition)
{
	size_t newOffset;
	switch(dwOrigin)
	{
	case STREAM_SEEK_SET:
		newOffset = 0;
		break;

	case STREAM_SEEK_CUR:
		newOffset = m_offset;
		break;

	case STREAM_SEEK_END:
		newOffset = m_pBuffer->GetLength();
		break;

	default:
		return E_INVALIDARG;
	}

	if(dlibMove.QuadPart < 0 && abs(dlibMove.QuadPart) > newOffset)
	{
		newOffset = 0;
	}
	else
	{
		newOffset = std::min(newOffset + dlibMove.QuadPart, (size_t)m_pBuffer->GetLength());
	}

	if(plibNewPosition != nullptr)
	{
		plibNewPosition->QuadPart = newOffset;
	}

	m_offset = newOffset;
	return S_OK;
}

HRESULT BufferStream::SetSize(ULARGE_INTEGER libNewSize)
{
	return E_NOTIMPL;
}

HRESULT BufferStream::CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten)
{
	HRESULT hr;
	auto ptr = (uint8_t*)m_pBuffer->GetPointer() + m_offset;
	auto m = std::min(m_offset + cb.QuadPart, (size_t)m_pBuffer->GetLength()) - m_offset;
	if(m == 0)
	{
		pcbWritten->QuadPart = 0;
		pcbRead->QuadPart = 0;
		return S_OK;
	}

	ULONG written = 0;
	hr = pstm->Write(ptr, m, &written);
	if(FAILED(hr))
	{
		pcbWritten->QuadPart = 0;
		pcbRead->QuadPart = 0;
		return hr;
	}

	pcbWritten->QuadPart = written;
	pcbRead->QuadPart = written;
	m_offset += written;
	return S_OK;
}

HRESULT BufferStream::Commit(DWORD grfCommitFlags)
{
	return E_NOTIMPL;
}

HRESULT BufferStream::Revert()
{
	return E_NOTIMPL;
}

HRESULT BufferStream::LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	return E_NOTIMPL;
}

HRESULT BufferStream::UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType)
{
	return E_NOTIMPL;
}

HRESULT BufferStream::Stat(STATSTG* pstatstg, DWORD grfStatFlag)
{
	pstatstg->type = STGTY_LOCKBYTES;
	pstatstg->cbSize.QuadPart = m_pBuffer->GetLength();
	pstatstg->grfLocksSupported = 0;
	
	return S_OK;
}

HRESULT BufferStream::Clone(IStream** ppstm)
{
	return E_NOTIMPL;
}
