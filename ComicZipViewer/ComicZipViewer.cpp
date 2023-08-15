#include "framework.h"
#include "ComicZipViewer.h"

#include "View.h"
#include "Model.h"
#include "NaturalSortOrder.h"
#include <wx/mstream.h>
#include <wx/wfstream.h>
wxIMPLEMENT_APP(ComicZipViewerApp);

ComicZipViewerApp::ComicZipViewerApp()
	: m_pView(nullptr)
	, m_pModel(nullptr)
	, m_pPageCollection(nullptr)
	, m_pSqlite(nullptr)
	, m_pStmtInsertLatestPage(nullptr)
	, m_pStmtSelectLatestPage(nullptr)
	, m_pStmtInsertBookmark(nullptr)
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
	wxFileName dataDirFileName(stdPaths.GetUserLocalDataDir(), wxS(""));
	dataDirFileName.AppendDir(wxS("thumbnail"));
	m_thunbnailDirPath = dataDirFileName.GetPath();
	wxFileName thumbnailDirName(m_thunbnailDirPath, wxS(""));
	if(!thumbnailDirName.IsDir())
	{
		if(wxMkDir(m_thunbnailDirPath) != 0)
			m_thunbnailDirPath.Clear();
	}
	
	m_pModel = new Model();
	m_pView = new View();
	m_pView->Show();
	return true;
}

int ComicZipViewerApp::OnExit()
{
	int ret = wxApp::OnExit();
	if(m_pStmtInsertLatestPage != nullptr)
	{
		sqlite3_finalize(m_pStmtInsertLatestPage);
	}

	if(m_pStmtSelectLatestPage != nullptr)
	{
		sqlite3_finalize(m_pStmtSelectLatestPage);
	}

	if(m_pStmtInsertBookmark != nullptr)
	{
		sqlite3_finalize(m_pStmtInsertBookmark);
	}

	if(m_pStmtSelectMarkedPages != nullptr)
	{
		sqlite3_finalize(m_pStmtSelectMarkedPages);
	}

	if(m_pStmtSelectAllPrefix != nullptr)
	{
		sqlite3_finalize(m_pStmtSelectAllPrefix);
	}

	if(m_pSqlite != nullptr)
	{
		sqlite3* pSqlite;
		auto& stdPathInfo = wxStandardPaths::Get();
		auto pathStr = stdPathInfo.GetUserLocalDataDir();
		wxFileName path(stdPathInfo.GetUserLocalDataDir(), wxS("bookmarks.sqlite3"));
		pathStr = path.GetFullPath();
		auto utf8Str = pathStr.ToUTF8();
		if(sqlite3_open_v2(utf8Str.data(), &pSqlite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE , nullptr) != SQLITE_OK)
			return false;

		sqlite3_backup* pBackup = sqlite3_backup_init(pSqlite, "main", m_pSqlite, "main");
		if(pBackup == nullptr)
			return false;

		(void)sqlite3_backup_step(pBackup, -1);
		(void)sqlite3_backup_finish(pBackup);
		sqlite3_close(pSqlite);
		sqlite3_close(m_pSqlite);
	}
	
	sqlite3_shutdown();
	if(sqlite3_temp_directory != nullptr)
		sqlite3_free(sqlite3_temp_directory);

	return ret;
}

int ComicZipViewerApp::OnRun()
{
	int ret = wxApp::OnRun();
	if(m_pPageCollection != nullptr)
	{
		InsertPageNameForReopen(m_pModel->openedPath, m_pModel->pageName);
	}

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

	if(m_pPageCollection != nullptr)
	{
		InsertPageNameForReopen(m_pModel->openedPath, m_pModel->pageName);
	}

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
	m_pModel->markedPageSet.clear();
	m_pModel->pageName = m_pModel->pageList.front();
	{
		int ret = 0;
		auto prefix = m_pModel->openedPath.ToUTF8();
		ret = sqlite3_bind_text(m_pStmtSelectLatestPage, 1, prefix.data(), -1, nullptr);
		assert(ret == SQLITE_OK);

		ret = sqlite3_step(m_pStmtSelectLatestPage);
		if(ret == SQLITE_DONE)
		{
			
		}
		else if(ret == SQLITE_ROW)
		{
			auto pageName = wxString::FromUTF8((const char*)sqlite3_column_text(m_pStmtSelectLatestPage, 0));
			auto it = std::find(m_pModel->pageList.begin(), m_pModel->pageList.end(), pageName);
			if(it != m_pModel->pageList.end())
			{
				m_pModel->pageName = pageName;
				m_pModel->currentPageNumber = it - m_pModel->pageList.begin();
			}
		}

		sqlite3_reset(m_pStmtSelectLatestPage);

		ret = sqlite3_bind_text(m_pStmtSelectMarkedPages, 1, prefix, -1, nullptr);
		assert(ret == SQLITE_OK);

		while((ret = sqlite3_step(m_pStmtSelectMarkedPages)) == SQLITE_ROW)
		{
			auto pageName = wxString::FromUTF8((const char*)sqlite3_column_text(m_pStmtSelectMarkedPages, 0));
			m_pModel->markedPageSet.insert(pageName);
		}

		sqlite3_reset(m_pStmtSelectMarkedPages);
	}



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

	if(loaded)
	{
		m_latestLoadedImageIdx = idx;
		m_latestLoadedImage = image;
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

void ComicZipViewerApp::AddMarked(int idx)
{
	wxImage image;
	const wxString& pageName = m_pModel->pageList[idx];
	if(m_pModel->markedPageSet.find(pageName) != m_pModel->markedPageSet.end())
		return;

	if(m_latestLoadedImageIdx != idx)
	{
		image = GetDecodedImage(idx);
		
	}
	else
	{
		image = m_latestLoadedImage;
	}

	int thumbnailWidth;
	int thumbnailHeight;
	auto imageSize = image.GetSize();
	if(imageSize.x > imageSize.y)
	{
		int ratio = (1024 * imageSize.y) / imageSize.x;
		thumbnailWidth = 256;
		thumbnailHeight = (ratio * 256) / 1024;
	}
	else
	{
		int ratio = (1024 * imageSize.x) / imageSize.y;
		thumbnailHeight = 256;
		thumbnailWidth = (ratio * 256) / 1024;
	}

	auto scaledImage = image.Scale(thumbnailWidth, thumbnailHeight);
	wxFileName thumbnailFileName(m_thunbnailDirPath, wxS(""));
	wxFile file;
	thumbnailFileName.AssignTempFileName(m_thunbnailDirPath + wxS("/"), &file);
	wxFileOutputStream oStream(file);
	scaledImage.SaveFile(oStream, wxBITMAP_TYPE_JPEG);
	auto thumbnailPath = thumbnailFileName.GetFullPath();
	sqlite3_bind_text16(m_pStmtInsertBookmark, 1, m_pModel->openedPath.wx_str(), -1, nullptr);
	sqlite3_bind_text16(m_pStmtInsertBookmark, 2, pageName.wx_str(), -1, nullptr);
	sqlite3_bind_text16(m_pStmtInsertBookmark, 3, thumbnailPath.wx_str(), -1, nullptr);
	int ret = sqlite3_step(m_pStmtInsertBookmark);
	assert(ret == SQLITE_DONE);

	sqlite3_reset(m_pStmtInsertBookmark);
	m_pModel->markedPageSet.insert(pageName);
}

bool ComicZipViewerApp::IsMarkedPage(int idx)
{
	auto& name = m_pModel->pageList[idx];
	return m_pModel->markedPageSet.find(name) != m_pModel->markedPageSet.end();
}

bool ComicZipViewerApp::InitalizeDatabase()
{
	if(sqlite3_initialize() != SQLITE_OK)
		return false;

	if(sqlite3_enable_shared_cache(true) != SQLITE_OK)
		return false;

	int ret = 0;
	sqlite3* pSqlite;
	auto& stdPathInfo = wxStandardPaths::Get();
	auto pathStr = stdPathInfo.GetUserLocalDataDir();
	if(!wxDirExists(pathStr))
		wxMkDir(pathStr);

	wxFileName path(stdPathInfo.GetUserLocalDataDir(), wxS("bookmarks.sqlite3"));
	pathStr = path.GetFullPath();
	auto utf8Str = pathStr.ToUTF8();
	if(sqlite3_open_v2(utf8Str.data(), &pSqlite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE , nullptr) != SQLITE_OK)
		return false;

	auto tempDbPath = wxFileName::CreateTempFileName(wxS("ComicZipViewer"));
	auto tempDbPathUtf8 = tempDbPath.ToUTF8();
	// copy to temporary db file
	sqlite3_open_v2(tempDbPathUtf8.data(), &m_pSqlite, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE  | SQLITE_OPEN_PRIVATECACHE, nullptr);
	sqlite3_backup* pBackup = sqlite3_backup_init(m_pSqlite, "main", pSqlite, "main");
	if(pBackup == nullptr)
		return false;

	int r1 = sqlite3_backup_step(pBackup, -1);
	int r2 = sqlite3_backup_finish(pBackup);
	sqlite3_close(pSqlite);
	// Check Tables
	sqlite3_stmt* pStmt;
	if(sqlite3_prepare(m_pSqlite, "PRAGMA user_version;", -1, &pStmt, nullptr) != SQLITE_OK)
		return false;

	if(sqlite3_step(pStmt) != SQLITE_ROW)
		return false;

	const int version = sqlite3_column_int(pStmt, 0);
	sqlite3_finalize(pStmt);
	if(version == 0)
	{
		// TODO: 
		static const char* const SQL_CREATE_BOOKMARKS =
R"(
CREATE TABLE tb_bookmarks_v1 (
	idx INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
	prefix TEXT KEY NOT NULL,
	page_name TEXT NOT NULL,
	thumbnail_path TEXT,
	UNIQUE(prefix, page_name)
);
)";

		static const char* const SQL_CREATE_PAGE_LAST_READ =
R"(
CREATE TABLE tb_page_last_read_v1(
	prefix TEXT PRIMARY KEY NOT NULL,
	page_name TEXT
);
)";

		char* errMsg;
		ret = sqlite3_exec(m_pSqlite, SQL_CREATE_BOOKMARKS, nullptr, nullptr, &errMsg);
		if(ret != SQLITE_OK)
			return false;

		ret = sqlite3_exec(m_pSqlite, SQL_CREATE_PAGE_LAST_READ, nullptr, nullptr, &errMsg);
		if(ret != SQLITE_OK)
			return false;

		ret = sqlite3_exec(m_pSqlite, "PRAGMA user_version=1;", nullptr, nullptr, &errMsg);
		if(ret != SQLITE_OK)
			return false;
		// Create Tables
		
		// ret = sqlite3_exec(m_pSqlite, , nullptr, nullptr, &errMsg);
	}
	else if(version == 1)
	{
		// TODO Check validation


	}
	else if(version < 1)
	{
		// in future, migrates tables
	}

	// WIP: create tables or migrates tables

	// Prepare SQL Queries

	static constexpr char SQL_INSERT_PAGE_LATEST_READ[] =
R"(
INSERT OR REPLACE
INTO tb_page_last_read_v1(
	prefix,
	page_name)
VALUES (
	?,
	?);
)";

	ret = sqlite3_prepare(m_pSqlite, SQL_INSERT_PAGE_LATEST_READ, sizeof(SQL_INSERT_PAGE_LATEST_READ) - 1, &m_pStmtInsertLatestPage, nullptr);
	if(ret != SQLITE_OK)
	{
		OutputDebugStringA(sqlite3_errmsg(m_pSqlite));
	}

	assert(ret == SQLITE_OK);

	static constexpr char SQL_SELECT_PAGE_LATEST_READ[] =
R"(
SELECT
	page_name
FROM
	tb_page_last_read_v1
WHERE
	prefix = ?;
)";
	ret = sqlite3_prepare(m_pSqlite, SQL_SELECT_PAGE_LATEST_READ, sizeof(SQL_SELECT_PAGE_LATEST_READ) - 1, &m_pStmtSelectLatestPage, nullptr);
	if(ret != SQLITE_OK)
	{
		OutputDebugStringA(sqlite3_errmsg(m_pSqlite));
	}

	assert(ret == SQLITE_OK);

	static constexpr char SQL_INSERT_BOOKMARK[] =
R"(
INSERT OR IGNORE
INTO
	tb_bookmarks_v1(
		prefix,
		page_name,
		thumbnail_path)
VALUES
	(?, ?, ?)
)";
	ret = sqlite3_prepare(m_pSqlite, SQL_INSERT_BOOKMARK, sizeof(SQL_INSERT_BOOKMARK) - 1, &m_pStmtInsertBookmark, nullptr);
	if(ret != SQLITE_OK)
	{
		OutputDebugStringA(sqlite3_errmsg(m_pSqlite));
	}
	assert(ret == SQLITE_OK);

	static constexpr char SQL_SELECT_MARKED_PAGES[] =
R"(
SELECT
	page_name
FROM
	tb_bookmarks_v1
WHERE
	prefix = ?
)";

	ret = sqlite3_prepare(m_pSqlite, SQL_SELECT_MARKED_PAGES, sizeof(SQL_SELECT_MARKED_PAGES) - 1, &m_pStmtSelectMarkedPages, nullptr);
	if(ret != SQLITE_OK)
	{
		OutputDebugStringA(sqlite3_errmsg(m_pSqlite));
	}
	assert(ret == SQLITE_OK);

	static constexpr char SQL_SELECT_ALL_PREFIX[] =
R"(
SELECT
	prefix,
	count(page_name)
FROM
	tb_bookmarks_v1
GROUP BY
	prefix
)";

	ret = sqlite3_prepare(m_pSqlite, SQL_SELECT_ALL_PREFIX, sizeof(SQL_SELECT_ALL_PREFIX) - 1, &m_pStmtSelectAllPrefix, nullptr);
	if(ret != SQLITE_OK)
	{
		OutputDebugStringA(sqlite3_errmsg(m_pSqlite));
	}
	assert(ret == SQLITE_OK);

	return true;
}

void ComicZipViewerApp::InsertPageNameForReopen(const wxString& prefix, const wxString& pageName)
{
	int ret;
	ret = sqlite3_bind_text16(m_pStmtInsertLatestPage, 1, prefix.wx_str(), -1, nullptr);
	assert(ret == SQLITE_OK);

	ret = sqlite3_bind_text16(m_pStmtInsertLatestPage, 2, pageName.wx_str(), -1, nullptr);
	assert(ret == SQLITE_OK);

	ret = sqlite3_step(m_pStmtInsertLatestPage);
	assert(ret == SQLITE_DONE);

	sqlite3_reset(m_pStmtInsertLatestPage);
}
