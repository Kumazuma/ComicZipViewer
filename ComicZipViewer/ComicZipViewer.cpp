#include "framework.h"
#include "ComicZipViewer.h"
#include "View.h"
#include "Model.h"
#include <wx/mstream.h>
wxIMPLEMENT_APP(ComicZipViewerApp);

ComicZipViewerApp::ComicZipViewerApp()
	: m_pView(nullptr)
	, m_pModel(nullptr)
	, m_pPageCollection(nullptr)
{
}

bool ComicZipViewerApp::OnInit()
{
	wxInitAllImageHandlers();
	m_pModel = new Model();
	m_pView = new View();
	m_pView->Show();
	return true;
}

int ComicZipViewerApp::OnRun()
{
	int ret = wxApp::OnRun();
	delete m_pView;
	delete m_pModel;
	delete m_pPageCollection;
	return ret;
}

bool ComicZipViewerApp::OpenFile(const wxString& filePath)
{
	auto newPageCollection = PageCollection::Create(filePath);
	if(newPageCollection == nullptr)
		return false;

	delete m_pPageCollection;
	m_pPageCollection = newPageCollection;
	// TODO: Reset cache
	m_pModel->openedPath = filePath;
	m_pModel->pageList.clear();
	m_pModel->currentPageNumber = 1;
	m_pPageCollection->GetPageNames(&m_pModel->pageList);
	m_pModel->pageName = m_pModel->pageList.front();
	return true;
}

uint32_t ComicZipViewerApp::GetPageCount() const
{
	return m_pModel->pageList.size();
}

bool ComicZipViewerApp::GetPageName(uint32_t idx, wxString* filename)
{
	if(m_pModel->pageList.size() <= idx)
		return false;

	*filename = *(m_pModel->pageList.data() + idx);
	return true;
}

constexpr uint64_t MAGIC_NUMBER_PNG = 0x89504e470d0a1a0a;
constexpr uint64_t MAGIC_NUMBER_GIF87A = 0x00474946383761; // GIF87a;
constexpr uint64_t MAGIC_NUMBER_GIF89A = 0x00474946383961; // GIF89a;
wxImage ComicZipViewerApp::GetDecodedImage(uint32_t idx)
{
	idx -= 1;
	wxImage image;
	if(m_pModel->pageList.size() <= idx)
		return {};

	// TODO: Not implemented yet Cache;
	auto& filename = *(m_pModel->pageList.data() + idx);
	auto buffer = m_pPageCollection->GetPage(filename);
	assert(buffer != nullptr);
	wxMemoryInputStream memStream(buffer->GetPointer(), buffer->GetLength());
	// Check png
	uint64_t masicNumber = 0;
	const uint32_t readByteCount = buffer->Copy(&masicNumber, 0, 8);
	bool loaded = false;
	masicNumber = wxUINT64_SWAP_ON_LE(masicNumber);
	if(readByteCount == 8 && masicNumber == MAGIC_NUMBER_PNG)
	{
		loaded  = m_pngHandler.LoadFile(&image, memStream, false);
	}
	
	if(!loaded && readByteCount >= 6)
	{
		const uint64_t masicNumber6Octet = masicNumber & (~0xFF00000000000000);
		if(masicNumber6Octet == MAGIC_NUMBER_GIF87A || masicNumber6Octet == MAGIC_NUMBER_GIF89A)
		{
			loaded = m_gifHandler.LoadFile(&image, memStream, false);
		}
	}
	// JPEG has no magic number
	if(!loaded && m_jpegHandler.CanRead(memStream))
	{
		loaded = m_jpegHandler.LoadFile(&image, memStream, false);
	}

	delete buffer;

	return image;
}

const wxString& ComicZipViewerApp::GetCurrentPageName() const
{
	return m_pModel->pageName;
}

int ComicZipViewerApp::GetCurrentPageNumber() const
{
	return m_pModel->currentPageNumber;
}
