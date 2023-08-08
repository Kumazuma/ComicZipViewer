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

private:
    ComicZipViewerFrame* m_pFrame;
};
