#include "framework.h"
#include "View.h"

#include "ComicZipViewer.h"
#include "ComicZipViewerFrame.h"
#include "BookmarksDialog.h"
#include "CustomCaptionFrame.h"

wxDEFINE_EVENT(wxEVT_OPEN_BOOKMARK , wxCommandEvent);

View::View()
	: m_pFrame(nullptr)
{

}

View::~View()
{
}

void View::Show()
{
	//auto& app = wxGetApp();
	//m_pFrame = new ComicZipViewerFrame();
	//if(!m_pFrame->Create(this))
	//	return;

	//m_pFrame->Show();
	//m_pFrame->SetRecentFiles(app.GetRecentReadBookAndPage());
	CustomCaptionFrame* pCap = new CustomCaptionFrame(nullptr, wxID_ANY, wxS("Test"));
	pCap->Show();
}

void View::OnMenu(wxCommandEvent& evt)
{
	const auto eventId = evt.GetId();
	if(eventId == wxID_OPEN)
	{
		wxFileDialog dialog(m_pFrame, wxS("Select a file"));
		dialog.SetWildcard(wxS("archive files(*.zip, *.cbz, *.tar, *.cbt)|*.zip;*.cbz;*.tar;*.cbt"));
		if (dialog.ShowModal() == wxID_CANCEL)
			return;

		auto path = dialog.GetPath();
		auto& app = wxGetApp();
		if (!app.OpenFile(path))
		{
			m_pFrame->ShowToast(wxS("해당 위치에 이미지 파일이 없거나, 파일을 열 수 없습니다."), true);
			return;
		}

		const int pageCount = app.GetPageCount();
		const int pageNumber = app.GetCurrentPageNumber();
		wxFileName fileName(app.GetPrefix());
		auto image = app.GetDecodedImage(app.GetCurrentPageNumber());
		m_pFrame->Freeze();
		m_pFrame->SetRecentFiles(app.GetRecentReadBookAndPage());
		m_pFrame->ShowImage(image);
		m_pFrame->SetTitle(wxString::Format(wxS("%s : %s [ %d / %d ] - ComicZipViewer") , fileName.GetFullName() , app.GetCurrentPageName() , pageNumber + 1 , pageCount));
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
	wxFileName fileName(app.GetPrefix());
	ComPtr<IWICBitmap> image = app.GetDecodedImage(pageNumber);
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("%s : %s [ %d / %d ] - ComicZipViewer") , fileName.GetFullName() , app.GetCurrentPageName() , pageNumber + 1 , pageCount));
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

	auto& currentPrefix = app.GetPrefix();
	if ( currentPrefix.IsEmpty() )
	{
		return;
	}

	ShowBook();
}

void View::OnClickedMovePrevBook(wxCommandEvent&)
{
	auto& app = wxGetApp();
	auto prefix = app.GetPrefix();
	do
	{
		prefix = app.GetPrevBook(prefix);
	} while ( !prefix.IsEmpty() && !app.OpenFile(prefix) );

	auto& currentPrefix = app.GetPrefix();
	if ( currentPrefix.IsEmpty() )
	{
		return;
	}
	
	ShowBook();
}

void View::OnForward(wxCommandEvent&)
{
	ComPtr<IWICBitmap> image;
	auto& app = wxGetApp();
	const int pageCount = app.GetPageCount();
	const int latestPageNumber = app.GetCurrentPageNumber();
	app.MoveNextPage();
	const int pageNumber = app.GetCurrentPageNumber();
	if ( pageNumber == latestPageNumber )
	{
		m_pFrame->ShowToast(wxS("마지막 페이지입니다."), true);
		return;
	}

	image = app.GetDecodedImage(pageNumber);
	wxFileName fileName(app.GetPrefix());
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);	m_pFrame->SetTitle(wxString::Format(wxS("%s : %s [ %d / %d ] - ComicZipViewer") , fileName.GetFullName() , app.GetCurrentPageName() , pageNumber + 1 , pageCount));
	m_pFrame->SetSeekBarPos(pageNumber);
	m_pFrame->SetPageIsMarked(false);
	if(app.IsMarkedPage(pageNumber))
		m_pFrame->SetPageIsMarked(true);

	m_pFrame->Thaw();
}

void View::OnBackward(wxCommandEvent&)
{
	ComPtr<IWICBitmap> image;
	auto& app = wxGetApp();
	const int pageCount = app.GetPageCount();
	const int latestPageNumber = app.GetCurrentPageNumber();
	app.MovePrevPage();
	int pageNumber = app.GetCurrentPageNumber();
	if ( pageNumber == latestPageNumber )
	{
		m_pFrame->ShowToast(wxS("첫 페이지입니다."), true);
		return;
	}

	image = app.GetDecodedImage(pageNumber);
	wxFileName fileName(app.GetPrefix());
	m_pFrame->Freeze();
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("%s : %s [ %d / %d ] - ComicZipViewer") , fileName.GetFullName() , app.GetCurrentPageName() , pageNumber + 1 , pageCount));
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
	BookmarksDialog dialog;
	if(!dialog.Create(m_pFrame, wxID_ANY, this, std::forward<decltype(bookmarks)>(bookmarks)))
		return;

	dialog.SetSize(m_pFrame->GetSize() / 2);
	dialog.CenterOnParent();
	if(dialog.ShowModal() != wxID_OK)
		return;

	const auto id = dialog.GetSelection();
	if(id == 0)
		return;

	app.OpenBookmark(id);
	ShowBook();
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
	ShowBook();
}

void View::OnMenuRecentFile(wxCommandEvent& evt)
{
	auto& app = wxGetApp();
	if (!app.OpenFile(evt.GetString()))
		return;

	ShowBook();
}

void View::ShowBook()
{
	auto& app = wxGetApp();
	auto currentPrefix = app.GetPrefix();
	wxFileName fileName(currentPrefix);
	const int pageCount = app.GetPageCount();
	const int currentPageNumber = app.GetCurrentPageNumber();
	auto image = app.GetDecodedImage(currentPageNumber);
	m_pFrame->Freeze();
	m_pFrame->SetRecentFiles(app.GetRecentReadBookAndPage());
	m_pFrame->ShowImage(image);
	m_pFrame->SetTitle(wxString::Format(wxS("%s : %s [ %d / %d ] - ComicZipViewer") , fileName.GetFullName() , app.GetCurrentPageName() , currentPageNumber + 1 , pageCount));
	m_pFrame->SetSeekBarPos(currentPageNumber);
	m_pFrame->SetPageIsMarked(false);
	if ( app.IsMarkedPage(currentPageNumber) )
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
	EVT_MENU_RANGE(ID_MENU_RECENT_FILE_ITEM_BEGIN, ID_MENU_RECENT_FILE_ITEM_END, View::OnMenuRecentFile)
	EVT_MENU(wxID_ANY , View::OnMenu)
END_EVENT_TABLE()
