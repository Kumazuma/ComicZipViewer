#include "framework.h"
#include "View.h"

#include "ComicZipViewer.h"
#include "ComicZipViewerFrame.h"
#include "BookmarksDialog.h"

wxDEFINE_EVENT(wxEVT_OPEN_BOOKMARK, wxCommandEvent);

View::View()
	: m_pFrame(nullptr)
{

}

View::~View()
{
}

void View::Show()
{
	m_pFrame = new ComicZipViewerFrame();
	if(!m_pFrame->Create(this))
		return;

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
		m_pFrame->SetSeekBarPos(pageNumber);
		m_pFrame->SetPageIsMarked(false);
		if(app.IsMarkedPage(pageNumber))
			m_pFrame->SetPageIsMarked(true);

		m_pFrame->Thaw();
	}
	else if(eventId == wxID_CLOSE)
	{
		m_pFrame->Close();
	}
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
	m_pFrame->SetPageIsMarked(false);
	if(app.IsMarkedPage(pageNumber))
		m_pFrame->SetPageIsMarked(true);

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

void View::OnClickedMark(wxCommandEvent&)
{
	auto& app = wxGetApp();
	app.AddMarked(app.GetCurrentPageNumber());
	m_pFrame->SetPageIsMarked(true);
}

void View::OnClickedMoveNextBook(wxCommandEvent&)
{
	auto& app = wxGetApp();
	auto prefix = app.GetPrefix();
	do
	{
		prefix = app.GetNextBook(prefix);
	} while ( !prefix.IsEmpty() && !app.OpenFile(prefix) );
	
	const int pageCount = app.GetPageCount();
	const int pageNumber = app.GetCurrentPageNumber();
	wxImage image = app.GetDecodedImage(app.GetCurrentPageNumber());
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s [%d/%d]") , app.GetCurrentPageName() , pageNumber + 1 , pageCount));
	m_pFrame->SetSeekBarPos(pageNumber);
	m_pFrame->SetPageIsMarked(false);
	if ( app.IsMarkedPage(pageNumber) )
		m_pFrame->SetPageIsMarked(true);

	m_pFrame->Thaw();
}

void View::OnClickedMovePrevBook(wxCommandEvent&)
{
	auto& app = wxGetApp();
	auto prefix = app.GetPrefix();
	do
	{
		prefix = app.GetPrevBook(prefix);
	} while ( !prefix.IsEmpty() && !app.OpenFile(prefix) );

	const int pageCount = app.GetPageCount();
	const int pageNumber = app.GetCurrentPageNumber();
	wxImage image = app.GetDecodedImage(app.GetCurrentPageNumber());
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s [%d/%d]") , app.GetCurrentPageName() , pageNumber + 1 , pageCount));
	m_pFrame->SetSeekBarPos(pageNumber);
	m_pFrame->SetPageIsMarked(false);
	if ( app.IsMarkedPage(pageNumber) )
		m_pFrame->SetPageIsMarked(true);

	m_pFrame->Thaw();
}

void View::OnForward(wxCommandEvent&)
{
	wxImage image;
	auto& app = wxGetApp();
	const int pageCount = app.GetPageCount();
	const int latestPageNumber = app.GetCurrentPageNumber();
	int pageNumber;
	do
	{
		app.MoveNextPage();
		pageNumber = app.GetCurrentPageNumber();
		if ( pageNumber == latestPageNumber )
		{
			return;
		}

		image = app.GetDecodedImage(pageNumber);
	} while ( !image.IsOk() );

	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s [%d/%d]") , app.GetCurrentPageName(), pageNumber + 1, pageCount));
	m_pFrame->SetSeekBarPos(pageNumber);
	m_pFrame->SetPageIsMarked(false);
	if(app.IsMarkedPage(pageNumber))
		m_pFrame->SetPageIsMarked(true);

	m_pFrame->Thaw();
}

void View::OnBackward(wxCommandEvent&)
{
	wxImage image;
	auto& app = wxGetApp();
	const int pageCount = app.GetPageCount();
	const int latestPageNumber = app.GetCurrentPageNumber();
	int pageNumber;
	do
	{
		app.MovePrevPage();
		pageNumber = app.GetCurrentPageNumber();
		if ( pageNumber == latestPageNumber )
		{
			return;
		}

		image = app.GetDecodedImage(pageNumber);
	} while ( !image.IsOk() );

	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s [%d/%d]") , app.GetCurrentPageName(), pageNumber + 1, pageCount));
	m_pFrame->SetSeekBarPos(pageNumber);
	m_pFrame->SetPageIsMarked(false);
	if(app.IsMarkedPage(pageNumber))
		m_pFrame->SetPageIsMarked(true);

	m_pFrame->Thaw();
}

void View::OnClickedBookmarks(wxCommandEvent&)
{
	auto& app = wxGetApp();
	auto bookmarks = app.GetAllMarkedPages();
	BookmarksDialog diloag;
	if(!diloag.Create(m_pFrame, wxID_ANY, this, std::forward<decltype(bookmarks)>(bookmarks)))
		return;

	diloag.CenterOnParent();

	if(diloag.ShowModal() != wxID_OK)
		return;

	const auto id = diloag.GetSelection();
	if(id == 0)
		return;

	app.OpenBookmark(id);
	const int pageNumber = app.GetCurrentPageNumber();
	const int pageCount = app.GetPageCount();
	wxImage image = app.GetDecodedImage(pageNumber);
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s [%d/%d]") , app.GetCurrentPageName(), pageNumber + 1, pageCount));
	m_pFrame->SetSeekBarPos(pageNumber);
	m_pFrame->SetPageIsMarked(false);
	if(app.IsMarkedPage(pageNumber))
		m_pFrame->SetPageIsMarked(true);

	m_pFrame->Thaw();
}

void View::OnOpenedBookmark(wxCommandEvent&)
{
	auto& app = wxGetApp();
	auto bookmarks = app.GetAllMarkedPages();
	BookmarksDialog diloag;
	if ( !diloag.Create(m_pFrame , wxID_ANY , this , std::forward<decltype( bookmarks )>(bookmarks)) )
		return;

	diloag.CenterOnParent();

	if ( diloag.ShowModal() != wxID_OK )
		return;

	const auto id = diloag.GetSelection();
	if ( id == 0 )
		return;

	app.OpenBookmark(id);
	const int pageNumber = app.GetCurrentPageNumber();
	const int pageCount = app.GetPageCount();
	wxImage image = app.GetDecodedImage(pageNumber);
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("ComicZipViewer: %s [%d/%d]") , app.GetCurrentPageName() , pageNumber + 1 , pageCount));
	m_pFrame->SetSeekBarPos(pageNumber);
	m_pFrame->SetPageIsMarked(false);
	if ( app.IsMarkedPage(pageNumber) )
		m_pFrame->SetPageIsMarked(true);

	m_pFrame->Thaw();
}

BEGIN_EVENT_TABLE(View , wxEvtHandler)
	EVT_MENU(ID_BTN_FIT_PAGE , View::OnClickedFitPage)
	EVT_MENU(ID_BTN_FIT_WIDTH , View::OnClickedFitWidth)
	EVT_MENU(ID_BTN_ORIGINAL , View::OnClickedOriginal)
	EVT_SCROLL_THUMBTRACK(View::OnSeek)
	EVT_BUTTON(wxID_FORWARD, View::OnForward)
	EVT_BUTTON(wxID_BACKWARD, View::OnBackward)
	EVT_BUTTON(ID_BTN_BOOKMARK_VIEW, View::OnClickedBookmarks)
	EVT_BUTTON(ID_BTN_ADD_MARK, View::OnClickedMark)
	EVT_BUTTON(ID_BTN_FIT_PAGE, View::OnClickedFitPage)
	EVT_BUTTON(ID_BTN_FIT_WIDTH, View::OnClickedFitWidth)
	EVT_BUTTON(ID_BTN_ORIGINAL, View::OnClickedOriginal)
	EVT_BUTTON(ID_BTN_MOVE_TO_NEXT_PAGE, View::OnClickedMoveNextBook)
	EVT_BUTTON(ID_BTN_MOVE_TO_PREV_PAGE , View::OnClickedMovePrevBook)
	EVT_COMMAND(wxID_ANY, wxEVT_OPEN_BOOKMARK, View::OnOpenedBookmark)
	EVT_MENU(wxID_ANY , View::OnMenu)
END_EVENT_TABLE()
