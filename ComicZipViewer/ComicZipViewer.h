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
	bool OnInit() override;
	int OnExit() override;
	int OnRun() override;
	Model& GetModel() {return *m_pModel; }
	bool OpenFile(const wxString& filePath);
	uint32_t GetPageCount() const;
	bool GetPageName(uint32_t idx, wxString* filename);
	wxImage GetDecodedImage(uint32_t idx);
	const wxString& GetCurrentPageName() const;
	int GetCurrentPageNumber() const;
	void MovePrevPage();
	void MoveNextPage();
	void MovePage(int idx);
	void AddMarked(int idx);
	bool IsMarkedPage(int idx);
private:
	bool InitalizeDatabase();
	void InsertPageNameForReopen(const wxString& prefix, const wxString& pageName);

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
};

wxDECLARE_APP(ComicZipViewerApp);
