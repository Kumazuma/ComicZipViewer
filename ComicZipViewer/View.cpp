#include "framework.h"
#include "View.h"

#include "ComicZipViewer.h"
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

void View::OnMenu(wxCommandEvent& evt)
{
	const auto eventId = evt.GetId();
	if(eventId == wxID_OPEN)
	{
		wxFileDialog dialog(m_pFrame, wxS("Select a file"));
		dialog.SetWildcard(wxS("zip files(*.zip)|*.zip"));
		if (dialog.ShowModal() == wxID_CANCEL)
			return;

		auto path = dialog.GetPath();
		auto& app = wxGetApp();
		if (!app.OpenFile(path))
			return;

		wxImage image = app.GetDecodedImage(app.GetCurrentPageNumber());
		m_pFrame->Freeze();
		m_pFrame->ShowImage(image);
		m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s"), app.GetCurrentPageName()));
		m_pFrame->SetSeekBarPos(0);
		m_pFrame->Thaw();
	}
	else if(eventId == wxID_CLOSE)
	{
		m_pFrame->Close();
	}
}

void View::OnClose(wxCloseEvent& evt)
{
	evt.Skip();
	m_pFrame->SetEventHandler(m_pFrame);
	SetNextHandler(nullptr);
	m_pFrame->Destroy();
}

void View::OnKeyDown(wxKeyEvent& evt)
{
	evt.Skip();
	if ( evt.GetEventObject() != m_pFrame )
		return;

	auto& app = wxGetApp();
	int latestPageNumber = app.GetCurrentPageNumber();
	switch( evt.GetKeyCode() )
	{
	case WXK_LEFT:
		app.MovePrevPage();
		break;
	case WXK_RIGHT:
		app.MoveNextPage();
		break;

	default:
		return;
	}

	if(app.GetCurrentPageNumber() == latestPageNumber)
	{

		return;
	}

	wxImage image = app.GetDecodedImage(app.GetCurrentPageNumber());
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s") , app.GetCurrentPageName()));
	m_pFrame->SetSeekBarPos(app.GetCurrentPageNumber());
	m_pFrame->Thaw();
}

void View::OnSeek(wxScrollEvent& evt)
{
	auto& app = wxGetApp();
	app.MovePage(evt.GetPosition());
	wxImage image = app.GetDecodedImage(app.GetCurrentPageNumber());
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s") , app.GetCurrentPageName()));
	m_pFrame->Thaw();
}

BEGIN_EVENT_TABLE(View , wxEvtHandler)
	EVT_MENU(wxID_ANY, View::OnMenu)
	EVT_CLOSE(View::OnClose)
	EVT_KEY_DOWN(View::OnKeyDown)
	EVT_SCROLL_THUMBTRACK(View::OnSeek)
END_EVENT_TABLE()
