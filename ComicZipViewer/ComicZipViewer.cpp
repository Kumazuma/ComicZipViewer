#include "framework.h"
#include "ComicZipViewer.h"

#include "View.h"
#include "Model.h"
#include "NaturalSortOrder.h"
#include <wx/mstream.h>
wxIMPLEMENT_APP(ComicZipViewerApp);

ComicZipViewerApp::ComicZipViewerApp()
	: m_pView(nullptr)
	, m_pModel(nullptr)
	, m_pPageCollection(nullptr)
	, m_pSqlite(nullptr)
{
}

bool ComicZipViewerApp::OnInit()
{
	if(!wxApp::OnInit())
		return false;

	SetAppName(wxS("ComicZipViewer"));
	SetVendorName(wxS("ROW"));

	wxInitAllImageHandlers();
	if(!InitalizeDatabase())
		return false;

	auto& stdPaths = wxStandardPaths::Get();
	wxFileName dataDirFileName(stdPaths.GetDataDir());
	dataDirFileName.AppendDir(wxS("thumbnail"));
	m_thunbnailDirPath = dataDirFileName.GetPath();
	if(wxMkDir(m_thunbnailDirPath) != 0)
		m_thunbnailDirPath.Clear();

	m_pModel = new Model();
	m_pView = new View();
	m_pView->Show();
	return true;
}

int ComicZipViewerApp::OnExit()
{
	int ret = wxApp::OnExit();
	if(m_pSqlite != nullptr)
		sqlite3_close(m_pSqlite);

	sqlite3_shutdown();
	if(sqlite3_temp_directory != nullptr)
		sqlite3_free(sqlite3_temp_directory);

	return ret;
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
	m_pModel->currentPageNumber = 0;
	m_pPageCollection->GetPageNames(&m_pModel->pageList);
	auto it = m_pModel->pageList.begin();
	while(it != m_pModel->pageList.end())
	{
		wxFileName fileName(*it);
		auto ext = fileName.GetExt().Lower();
		bool isImageFile = true;
		do
		{
			if(ext == wxS("jpg"))
				break;

			if(ext == wxS("jpeg"))
				break;

			if(ext == wxS("jfif"))
				break;

			if(ext == wxS("png"))
				break;

			if(ext == wxS("gif"))
				break;

			isImageFile = false;
		} while(false);

		if(isImageFile)
		{
			it += 1;
		}
		else
		{
			it = m_pModel->pageList.erase(it);
		}
	}

	if(m_pModel->pageList.empty())
	{
		delete m_pPageCollection;
		m_pPageCollection = nullptr;
		return false;
	}

	std::sort(m_pModel->pageList.begin(), m_pModel->pageList.end(), NaturalSortOrder{});
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
constexpr uint64_t MAGIC_NUMBER_GIF87A = 0x4749463837610000; // GIF87a;
constexpr uint64_t MAGIC_NUMBER_GIF89A = 0x4749463839610000; // GIF89a;
wxImage ComicZipViewerApp::GetDecodedImage(uint32_t idx)
{
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

	// JPEG has no magic number
	if(!loaded && m_jpegHandler.CanRead(memStream))
	{
		loaded = m_jpegHandler.LoadFile(&image, memStream, false);
	}

	if(!loaded && readByteCount >= 6)
	{
		const uint64_t masicNumber6Octet = masicNumber & (~0x000000000000FFFF);
		if(masicNumber6Octet == MAGIC_NUMBER_GIF87A || masicNumber6Octet == MAGIC_NUMBER_GIF89A)
		{
			loaded = m_gifHandler.LoadFile(&image, memStream, false);
		}
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

void ComicZipViewerApp::MovePrevPage()
{
	if ( m_pModel->currentPageNumber == 0 || m_pModel->pageList.empty())
		return;

	m_pModel->currentPageNumber -= 1;
	m_pModel->pageName = m_pModel->pageList[m_pModel->currentPageNumber ];
}

void ComicZipViewerApp::MoveNextPage()
{
	int num = m_pModel->currentPageNumber + 1;
	if ( m_pModel->pageList.size() <= num || m_pModel->pageList.empty() )
		return;

	m_pModel->currentPageNumber = num;
	m_pModel->pageName = m_pModel->pageList[ m_pModel->currentPageNumber ];
}

void ComicZipViewerApp::MovePage(int idx)
{
	if ( idx < 0 )
		return;

	if ( m_pModel->pageList.size() <= idx )
		return;

	m_pModel->currentPageNumber = idx;
	m_pModel->pageName = m_pModel->pageList[ m_pModel->currentPageNumber ];
}

bool ComicZipViewerApp::InitalizeDatabase()
{
	if(sqlite3_initialize() != SQLITE_OK)
		return false;

	if(sqlite3_enable_shared_cache(true) != SQLITE_OK)
		return false;

	int ret = 0;
	auto& stdPathInfo = wxStandardPaths::Get();
	auto path = stdPathInfo.GetUserDataDir();
	path.Append(wxS("bookmarks.sqlite3"));
	auto utf8Str = path.ToUTF8();
	wxFileName tempPath(stdPathInfo.GetUserDataDir());
	tempPath.AppendDir(wxS("temp"));
	auto tempPathStr = tempPath.GetPath();
	wxMkDir(tempPathStr);
	auto tempPathUtf8 = tempPathStr.ToUTF8();
	sqlite3_win32_set_directory8(SQLITE_WIN32_TEMP_DIRECTORY_TYPE, tempPathUtf8.data());
	if(sqlite3_open_v2(utf8Str.data(), &m_pSqlite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE , nullptr) != SQLITE_OK)
		return false;

	// Check Tables
	sqlite3_stmt* pStmt;
	if(sqlite3_prepare(m_pSqlite, "PRAGMA user_version;", -1, &pStmt, nullptr) != SQLITE_OK)
		return false;

	if(sqlite3_step(pStmt) != SQLITE_OK)
		return false;

	const int version = sqlite3_column_int(pStmt, 0);
	sqlite3_finalize(pStmt);
	if(version == 0)
	{
		// TODO: 
		// static const char* 
		// Create Tables
		char* errMsg;
		// ret = sqlite3_exec(m_pSqlite, , nullptr, nullptr, &errMsg);
	}
	else
	{
		// Net now


	}

	// WIP: create tables or migrates tables

	return true;
}
