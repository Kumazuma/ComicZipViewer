#pragma once
#include <wx/wx.h>

class ComicZipViewerFrame;
class BookmarksDialog;
class View: public wxEvtHandler
{
	DECLARE_EVENT_TABLE();
public:
	View();
	~View();
	void Show();
	
protected:
	void OnMenu(wxCommandEvent& evt);
	void OnSeek(wxScrollEvent& evt);
	void OnClickedOriginal(wxCommandEvent&);
	void OnClickedFitPage(wxCommandEvent&);
	void OnClickedFitWidth(wxCommandEvent&);
	void OnClickedMark(wxCommandEvent&);
	void OnClickedMoveNextBook(wxCommandEvent&);
	void OnClickedMovePrevBook(wxCommandEvent&);
	void OnForward(wxCommandEvent&);
	void OnBackward(wxCommandEvent&);
	void OnClickedBookmarks(wxCommandEvent&);
	void OnOpenedBookmark(wxCommandEvent&);
	void OnMenuRecentFile(wxCommandEvent&);
private:
	ComicZipViewerFrame* m_pFrame;
};

wxDECLARE_EVENT(wxEVT_OPEN_BOOKMARK, wxCommandEvent);
