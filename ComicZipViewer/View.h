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
	void OnForward(wxCommandEvent&);
	void OnBackward(wxCommandEvent&);
	void OnClickedBookmarks(wxCommandEvent&);
private:
	ComicZipViewerFrame* m_pFrame;
	BookmarksDialog* m_pBookMarkDialog;
};
