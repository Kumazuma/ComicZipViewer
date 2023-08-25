#pragma once
#include "framework.h"
#include <wx/wx.h>
#include "PageCollection.h"
#include "sqlite3.h"

class View;
struct Model;
class ComicZipViewerApp: public wxApp
{
public:
	ComicZipViewerApp();
	ComicZipViewerApp(const ComicZipViewerApp&) = delete;
	bool OnInit() override;
	int OnExit() override;
	int OnRun() override;
	Model& GetModel() {return *m_pModel; }
	bool OpenFile(const wxString& filePath);
	uint32_t GetPageCount() const;
	bool GetPageName(uint32_t idx, wxString* filename);
	wxImage GetDecodedImage(uint32_t idx);
	const wxString& GetPrefix() const;
	const wxString& GetCurrentPageName() const;
	int GetCurrentPageNumber() const;
	void MovePrevPage();
	void MoveNextPage();
	void MovePage(int idx);
	void MovePage(const wxString& pageName);
	void AddMarked(int idx);
	bool IsMarkedPage(int idx);
	void OpenBookmark(int idx);
	std::vector<std::tuple<int, wxString, wxString>> GetAllMarkedPages();
	wxString GetNextBook(const wxString& prefix);
	wxString GetPrevBook(const wxString& prefix);
	std::vector<wxString> GetBookListInParentDir(const wxString& parentPath);
	std::vector<std::tuple<wxString, wxString>> GetRecentReadBookAndPage() const;
private:
	bool InitializeDatabase();
	void InsertPageNameForReopen(const wxString& prefix, const wxString& pageName);
	bool Open(const wxString& filePath);
private:
	View* m_pView;
	Model* m_pModel;
	PageCollection* m_pPageCollection;
	wxPNGHandler m_pngHandler;
	wxGIFHandler m_gifHandler;
	wxJPEGHandler m_jpegHandler;
	wxImage m_latestLoadedImage;
	sqlite3* m_pSqlite;
	wxString m_thunbnailDirPath;
	int m_latestLoadedImageIdx;
	sqlite3_stmt* m_pStmtInsertLatestPage;
	sqlite3_stmt* m_pStmtSelectLatestPage;
	sqlite3_stmt* m_pStmtInsertBookmark;
	sqlite3_stmt* m_pStmtSelectMarkedPages;
	sqlite3_stmt* m_pStmtSelectAllPrefix;
	sqlite3_stmt* m_pStmtSelectAllMarkedPages;
	sqlite3_stmt* m_pStmtSelectMarkedPage;
	sqlite3_stmt* m_pStmtSelectLatestReadBookAndPage;
};

wxDECLARE_APP(ComicZipViewerApp);
