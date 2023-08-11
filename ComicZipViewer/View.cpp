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

		const int pageCount = app.GetPageCount();
		const int pageNumber = app.GetCurrentPageNumber();
		wxImage image = app.GetDecodedImage(app.GetCurrentPageNumber());
		m_pFrame->Freeze();
		m_pFrame->ShowImage(image);
		m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s [%d/%d]") , app.GetCurrentPageName(), pageNumber + 1, pageCount));
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

	const int pageCount = app.GetPageCount();
	const int pageNumber = app.GetCurrentPageNumber();
	if(pageNumber == latestPageNumber)
	{
		return;
	}

	wxImage image = app.GetDecodedImage(pageNumber);
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s [%d/%d]") , app.GetCurrentPageName(), pageNumber + 1, pageCount));
	m_pFrame->SetSeekBarPos(pageNumber);
	m_pFrame->Thaw();
}

void View::OnSeek(wxScrollEvent& evt)
{
	auto& app = wxGetApp();
	app.MovePage(evt.GetPosition());
	const int pageCount = app.GetPageCount();
	const int pageNumber = app.GetCurrentPageNumber();
	wxImage image = app.GetDecodedImage(pageNumber);
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s [%d/%d]") , app.GetCurrentPageName(), pageNumber + 1, pageCount));
	m_pFrame->Thaw();
}

void View::OnClickedOriginal(wxCommandEvent&)
{
	m_pFrame->SetImageViewMode(ImageViewModeKind::ORIGINAL);
}

void View::OnClickedFitPage(wxCommandEvent&)
{
	m_pFrame->SetImageViewMode(ImageViewModeKind::FIT_PAGE);
}

void View::OnClickedFitWidth(wxCommandEvent&)
{
	m_pFrame->SetImageViewMode(ImageViewModeKind::FIT_WIDTH);
}

void View::OnForward(wxCommandEvent&)
{
	auto& app = wxGetApp();
	int latestPageNumber = app.GetCurrentPageNumber();
	app.MoveNextPage();
	const int pageCount = app.GetPageCount();
	const int pageNumber = app.GetCurrentPageNumber();
	if ( pageNumber == latestPageNumber )
	{
		return;
	}

	wxImage image = app.GetDecodedImage(pageNumber);
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s [%d/%d]") , app.GetCurrentPageName(), pageNumber + 1, pageCount));
	m_pFrame->SetSeekBarPos(pageNumber);
	m_pFrame->Thaw();
}

void View::OnBackward(wxCommandEvent&)
{
	auto& app = wxGetApp();
	const int pageCount = app.GetPageCount();
	const int latestPageNumber = app.GetCurrentPageNumber();
	app.MovePrevPage();
	const int pageNumber = app.GetCurrentPageNumber();
	if ( pageNumber == latestPageNumber )
	{
		return;
	}

	wxImage image = app.GetDecodedImage(pageNumber);
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s [%d/%d]") , app.GetCurrentPageName(), pageNumber + 1, pageCount));
	m_pFrame->SetSeekBarPos(pageNumber);
	m_pFrame->Thaw();
}

BEGIN_EVENT_TABLE(View, wxEvtHandler)
	EVT_MENU(ID_BTN_FIT_PAGE , View::OnClickedFitPage)
	EVT_MENU(ID_BTN_FIT_WIDTH , View::OnClickedFitWidth)
	EVT_MENU(ID_BTN_ORIGINAL , View::OnClickedOriginal)
	EVT_CLOSE(View::OnClose)
	EVT_KEY_DOWN(View::OnKeyDown)
	EVT_SCROLL_THUMBTRACK(View::OnSeek)
	EVT_BUTTON(wxID_FORWARD, View::OnForward)
	EVT_BUTTON(wxID_BACKWARD, View::OnBackward)
	EVT_BUTTON(ID_BTN_FIT_PAGE, View::OnClickedFitPage)
	EVT_BUTTON(ID_BTN_FIT_WIDTH, View::OnClickedFitWidth)
	EVT_BUTTON(ID_BTN_ORIGINAL, View::OnClickedOriginal)
	EVT_MENU(wxID_ANY , View::OnMenu)
END_EVENT_TABLE()
