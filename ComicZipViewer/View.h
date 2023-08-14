#pragma once
#include <wx/wx.h>

class ComicZipViewerFrame;
class View: public wxEvtHandler
{
	DECLARE_EVENT_TABLE();
public:
	View();
	void Show();
	
protected:
	void OnMenu(wxCommandEvent& evt);
	void OnClose(wxCloseEvent& evt);
	void OnKeyDown(wxKeyEvent& evt);
	void OnSeek(wxScrollEvent& evt);
	void OnClickedOriginal(wxCommandEvent&);
	void OnClickedFitPage(wxCommandEvent&);
	void OnClickedFitWidth(wxCommandEvent&);
	void OnClickedMark(wxCommandEvent&);
	void OnForward(wxCommandEvent&);
	void OnBackward(wxCommandEvent&);
private:
	ComicZipViewerFrame* m_pFrame;
};
