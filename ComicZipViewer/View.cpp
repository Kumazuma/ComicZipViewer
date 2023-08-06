#include "framework.h"
#include "View.h"
#include "ComicZipViewerFrame.h"

View::View()
    : m_pFrame(nullptr)
{

}

void View::Show()
{
    m_pFrame = new ComicZipViewerFrame();
    if(!m_pFrame->Create())
        return;

    SetNextHandler(m_pFrame);
    m_pFrame->SetEventHandler(this);
    m_pFrame->Show();
}

void View::OnOpenFile(wxCommandEvent& evt)
{
    wxFileDialog dialog(m_pFrame, wxS("Select a file"));
    dialog.SetWildcard(wxS("zip files(*.zip)|*.zip"));
    if(dialog.ShowModal() == wxID_CANCEL)
        return;

    auto path = dialog.GetPath();
    wxMessageBox(path);
}

void View::OnClose(wxCloseEvent& evt)
{
    evt.Skip();
    m_pFrame->SetEventHandler(m_pFrame);
    SetNextHandler(nullptr);
}

BEGIN_EVENT_TABLE(View, wxEvtHandler)
EVT_MENU(wxID_OPEN, View::OnOpenFile)
EVT_CLOSE(View::OnClose)
END_EVENT_TABLE()