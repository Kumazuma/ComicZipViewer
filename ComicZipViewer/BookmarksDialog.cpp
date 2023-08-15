#include "framework.h"
#include "BookmarksDialog.h"
#include <wx/graphics.h>

bool BookmarksDialog::Create(wxWindow* parent, wxWindowID winId, wxEvtHandler* pView)
{
	m_pView = pView;
	wxDialog::Create(parent, winId, wxS("Bookmarks"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);


	return true;
}

WXLRESULT BookmarksDialog::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
{
	switch(message)
	{
	case WM_ENTERSIZEMOVE:
		m_isSizing = true;
		break;
	case WM_EXITSIZEMOVE:
		m_isSizing = false;
		//UpdateClientSize(GetClientSize());
		//ResizeSwapChain(m_clientSize);
		//Render();
		//{
		//	wxClientDC dc(this);
		//	wxMemoryDC memDC(m_backBuffer);
		//	dc.Blit(wxPoint(0, 0), m_backBuffer.GetSize(), &memDC, wxPoint(0, 0));
		//}
		break;
	}

	return wxDialog::MSWWindowProc(message , wParam , lParam);
}

void BookmarksDialog::OnPaintEvent(wxPaintEvent&)
{
	m_onPainting = true;
	wxPaintDC paintDC(this);
	wxMemoryDC memDC(m_backBuffer);
	paintDC.Blit(wxPoint(0, 0), m_backBuffer.GetSize(), &memDC, wxPoint(0, 0));
	m_onPainting = false;
}

void BookmarksDialog::OnSizeEvent(wxSizeEvent&)
{
	if(m_isSizing)
		return;

	UpdateClientSize(GetClientSize());
	ResizeSwapChain(m_clientSize);
	Render();
}

void BookmarksDialog::UpdateClientSize(const wxSize& size)
{
	m_clientSize = size;
}

void BookmarksDialog::ResizeSwapChain(const wxSize& size)
{
	m_backBuffer = wxBitmap(size.x, size.y);
}

void BookmarksDialog::Render()
{
	if(IsFrozen())
		return;

	wxMemoryDC memDC(m_backBuffer);
	auto d2dContext = wxGraphicsRenderer::GetDirect2DRenderer();
	auto context = d2dContext->CreateContext(memDC);
	context->ClearRectangle(0.0, 0.0, m_clientSize.x, m_clientSize.y);
	delete context;
}

BEGIN_EVENT_TABLE(BookmarksDialog , wxDialog)
//EVT_PAINT(BookmarksDialog::OnPaintEvent)
//EVT_SIZE(BookmarksDialog::OnSizeEvent)
END_EVENT_TABLE()
