#include "framework.h"
#include "CustomCaptionFrame.h"
#include <uxtheme.h>
#include <Vsstyle.h>

inline static int GetScaleUsingDpi(int value, int dpi)
{
	return ((value * 16) * ((dpi * 16) / 96)) / 16;
}

CustomCaptionFrame::CustomCaptionFrame()
: m_currentHoveredButtonId(wxID_ANY)
, m_borderPadding()
, m_borderThickness()
, m_willRender()
{
}

CustomCaptionFrame::CustomCaptionFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos,
	const wxSize& size, long style, const wxString& name)
: CustomCaptionFrame() 
{
	CustomCaptionFrame::Create(parent , id , title , pos , size , style , name);
}

bool CustomCaptionFrame::Create(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos,
	const wxSize& size, long style, const wxString& name)
{
	if(!wxFrame::Create(parent, id, title, pos, size, style, name) )
	{
		return false;
	}

	int dpi = GetDPI().x;
	UpdateCaptionDesc(dpi);
	m_renderSystem.Initalize(this, GetClientSize());
	return true;
}

WXLRESULT CustomCaptionFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
	switch(nMsg)
	{
	case WM_SIZE:
		UpdateCaptionDesc(GetDPI().x);
	{
		RECT rect;
		::GetClientRect(GetHWND(), &rect);
		m_renderSystem.ResizeSwapChain(wxSize{rect.right - rect.left, rect.bottom - rect.top});
		TryRender();
	}
		
		break;

	case WM_NCCALCSIZE:
		return OnNcCalcSize(nMsg, wParam, lParam);

	case WM_NCHITTEST:
		return OnNcHitTest(nMsg, wParam, lParam);

	case WM_NCLBUTTONDOWN:
		return OnNcLButtonDown(nMsg, wParam, lParam);

	case WM_NCLBUTTONUP:
		return OnNcLButtonUp(nMsg, wParam, lParam);

	case WM_NCMOUSEMOVE:
		return OnNcMouseMove(nMsg, wParam, lParam);

	case WM_NCMOUSELEAVE:
		return OnNcMouseLeave(nMsg, wParam, lParam);

	case WM_NCMOUSEHOVER:
		return OnNcMouseEnter(nMsg, wParam, lParam);
	}

	return wxFrame::MSWWindowProc(nMsg , wParam , lParam);
}

void CustomCaptionFrame::MSWUpdateFontOnDPIChange(const wxSize& newDPI)
{
	wxFrame::MSWUpdateFontOnDPIChange(newDPI);
	UpdateCaptionDesc(newDPI.x);
	TryRender();
}

void CustomCaptionFrame::Render()
{
	m_willRender = false;
	if(IsFrozen())
		return;

	ComPtr<ID2D1DeviceContext1> d2dContext;
	ComPtr<IDXGISwapChain1> swapchain;
	m_renderSystem.GetD2DDeviceContext(&d2dContext);
	m_renderSystem.GetSwapChain(&swapchain);

	d2dContext->BeginDraw();
	d2dContext->Clear(D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.f));
	DoRender();
	RenderCaption();
	d2dContext->EndDraw();
	swapchain->Present(1, 0);
}

void CustomCaptionFrame::TryRender()
{
	
	if(IsFrozen() || m_willRender)
	{
		return;
	}

	m_willRender = true;
	CallAfter(&CustomCaptionFrame::DeferredRender);
}

void CustomCaptionFrame::DeferredRender()
{
	if(!m_willRender)
		return;

	Render();
}

void win32_center_rect_in_rect(
  D2D1_RECT_F *to_center,
  const D2D1_RECT_F& outer_rect
) {
  const auto to_width = to_center->right - to_center->left;
  const auto to_height = to_center->bottom - to_center->top;
  const auto outer_width = outer_rect.right - outer_rect.left;
  const auto outer_height = outer_rect.bottom - outer_rect.top;

  const auto padding_x = (outer_width - to_width) / 2.f;
  const auto padding_y = (outer_height - to_height) / 2.f;

  to_center->left = outer_rect.left + padding_x;
  to_center->top = outer_rect.top + padding_y;
  to_center->right = to_center->left + to_width;
  to_center->bottom = to_center->top + to_height;
}

inline D2D1_RECT_F GetRect(wxRect& rc)
{
	return D2D1::RectF(
		(float)rc.GetLeft(),
		(float)rc.GetTop(),
		(float)rc.GetRight(),
		(float)rc.GetBottom()
	);
}

void CustomCaptionFrame::RenderCaption()
{
	ComPtr<ID2D1SolidColorBrush> hoveredColorBrush;
	ComPtr<ID2D1SolidColorBrush> buttonColorBrush;
	ComPtr<ID2D1SolidColorBrush> whiteColorBrush;
	ComPtr<ID2D1DeviceContext1> context;
	D2D1::ColorF buttonColor(127.f / 255.f, 127.f / 255.f, 127.f / 255.f, 1.f);
	if(HasFocus())
	{
		buttonColor = D2D1::ColorF(33.f / 255.f, 33.f / 255.f, 33.f / 255.f, 1.f);
	}

	m_renderSystem.GetSolidColorBrush(D2D1::ColorF(130.f / 255.f, 130.f / 255.f, 130.f / 255.f, 1.f), &hoveredColorBrush);
	m_renderSystem.GetSolidColorBrush(buttonColor, &buttonColorBrush);
	m_renderSystem.GetSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &whiteColorBrush);
	m_renderSystem.GetD2DDeviceContext(&context);
	// Paint Title Bar
	const int dpi = GetDPI().x;
	float icon_dimension = GetScaleUsingDpi(10, dpi) / 16.f;
	wxRect* rc = nullptr;
	switch(m_currentHoveredButtonId)
	{
	case wxID_MAXIMIZE_BOX:
		rc = &m_titleBarButtonRects.maximize;
		break;

	case wxID_MINIMIZE_BOX:
		rc = &m_titleBarButtonRects.minimize;
		break;

	case wxID_CLOSE_BOX:
		rc = &m_titleBarButtonRects.close;
		break;
	}

	if(rc != nullptr)
	{
		auto rcf = D2D1::RectF(
			(float)rc->GetLeft(),
			(float)rc->GetTop(),
			(float)rc->GetRight(),
			(float)rc->GetBottom()
		);

		context->FillRectangle(rcf, hoveredColorBrush.Get());
	}

	D2D1_RECT_F icon_rect{ };
	icon_rect.right = icon_dimension;
	icon_rect.bottom = 2.f;
	win32_center_rect_in_rect(&icon_rect, ::GetRect(m_titleBarButtonRects.minimize));
	context->FillRectangle(icon_rect, buttonColorBrush.Get());

	icon_rect.top = 0.f;
	icon_rect.left = 0.f;
	icon_rect.right = icon_dimension;
	icon_rect.bottom = icon_dimension;
	win32_center_rect_in_rect(&icon_rect, ::GetRect(m_titleBarButtonRects.maximize));
	if(IsMaximized())
	{
		auto rc = icon_rect;
		rc .left += MAXIMIZED_RECTANGLE_OFFSET;
		rc .top -= MAXIMIZED_RECTANGLE_OFFSET;
		rc .right += MAXIMIZED_RECTANGLE_OFFSET;
		rc .bottom -= MAXIMIZED_RECTANGLE_OFFSET;
		context->DrawRectangle(rc, buttonColorBrush.Get(), 2.f);
	}

	context->DrawRectangle(icon_rect, buttonColorBrush.Get(), 2.f);
	
	icon_rect.top = 0.f;
	icon_rect.left = 0.f;
	icon_rect.right = icon_dimension;
	icon_rect.bottom = icon_dimension;
	win32_center_rect_in_rect(&icon_rect, ::GetRect(m_titleBarButtonRects.close));
	ID2D1SolidColorBrush* pBrush = buttonColorBrush.Get();
	if(m_currentHoveredButtonId == wxID_CLOSE_BOX)
	{
		pBrush = whiteColorBrush.Get();
	}

	context->DrawLine(D2D1::Point2F(icon_rect.left, icon_rect.top), D2D1::Point2F(icon_rect.right + 1.f, icon_rect.bottom + 1), pBrush, 2.f);
	context->DrawLine(D2D1::Point2F(icon_rect.left, icon_rect.bottom), D2D1::Point2F(icon_rect.right + 1.f, icon_rect.top - 1), pBrush, 2.f);
}

void CustomCaptionFrame::DoThaw()
{
	wxFrame::DoThaw();
	Render();
}

WXLRESULT CustomCaptionFrame::OnNcCalcSize(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	if(wParam == 0)
	{
		return wxFrame::MSWWindowProc(nMsg , wParam , lParam);
	}

	NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lParam;
	RECT* requestedClientRect = params->rgrc;

	requestedClientRect->right -= m_borderThickness.x + m_borderPadding;
	requestedClientRect->left += m_borderThickness.x + m_borderPadding;
	requestedClientRect->bottom -= m_borderThickness.y + m_borderPadding;
	if (IsMaximized()) 
	{
		requestedClientRect->top += m_borderPadding;
	}

	return 0;
}

WXLRESULT CustomCaptionFrame::OnNcHitTest(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	WXLRESULT hitTestRet = MSWDefWindowProc(nMsg, wParam, lParam);
	// 10 11 12 13 14 15 16 17
	if(hitTestRet == HTNOWHERE
		// HTLEFT HTRIGHT HTTOP HTTBOTTOM HTTOPLEFT HTTOPRIGHT HTBOTTOMLEFT HTBOTTOMRIGHT
		|| (HTLEFT <= hitTestRet && hitTestRet <= HTBOTTOMRIGHT))
	{
		return hitTestRet;
	}

	// Check if hover button is on maximize to support SnapLayout on Windows 11
	if(m_currentHoveredButtonId == wxID_MAXIMIZE_BOX)
	{
		return HTMAXBUTTON;
	}

	// Looks like adjustment happening in NCCALCSIZE is messing with the detection
	// of the top hit area so manually fixing that.
	wxPoint cursor_point(LOWORD(lParam), HIWORD(lParam));
	cursor_point = ScreenToClient(cursor_point);
	if (cursor_point.y > 0
		&& cursor_point.y < m_borderThickness.y + m_borderPadding)
	{
		return HTTOP;
	}

	// Since we are drawing our own caption, this needs to be a custom test
	if (cursor_point.y < m_titleBarRect.GetBottom())
	{
		return HTCAPTION;
	}
	
	return HTCLIENT;
}

WXLRESULT CustomCaptionFrame::OnNcLButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
// Clicks on buttons will be handled in WM_NCLBUTTONUP, but we still need
// to remove default handling of the click to avoid it counting as drag.
//
// Ideally you also want to check that the mouse hasn't moved out or too much
// between DOWN and UP messages.
	if(m_currentHoveredButtonId != wxID_ANY)
		return 0;

	return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
}

WXLRESULT CustomCaptionFrame::OnNcLButtonUp(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	switch(m_currentHoveredButtonId)
	{
	case wxID_MINIMIZE_BOX:
		::ShowWindow(GetHWND(), SW_MINIMIZE);
		return 0;

		case wxID_CLOSE_BOX:
		::PostMessageW(GetHWND(), WM_CLOSE, 0, 0);
		return 0;

	case wxID_MAXIMIZE_BOX:
		::ShowWindow(GetHWND(), IsMaximized() ? SW_NORMAL : SW_MAXIMIZE);
		return 0;
	}

	return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
}

WXLRESULT CustomCaptionFrame::OnNcMouseMove(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	wxPoint cursor = wxGetMousePosition();
	ScreenToClient(&cursor.x, &cursor.y);
	wxWindowID id = wxID_ANY;
	if (m_titleBarButtonRects.minimize.GetX() <= cursor.x
		&& m_titleBarButtonRects.minimize.GetBottom() >= cursor.y)
	{
		if (m_titleBarButtonRects.close.Contains(cursor))
		{
			id = wxID_CLOSE_BOX;
		}
		else if(m_titleBarButtonRects.minimize.Contains(cursor))
		{
			id = wxID_MINIMIZE_BOX;
		}
		else if(m_titleBarButtonRects.maximize.Contains(cursor))
		{
			id = wxID_MAXIMIZE_BOX;
		}
	}

	if(m_currentHoveredButtonId != id)
	{
		TryRender();
	}

	m_currentHoveredButtonId = id;

	return wxFrame::MSWWindowProc(nMsg, wParam, lParam);

}

WXLRESULT CustomCaptionFrame::OnNcMouseLeave(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
}

WXLRESULT CustomCaptionFrame::OnNcMouseEnter(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
}

void CustomCaptionFrame::UpdateCaptionDesc(int dpi)
{
	m_borderThickness.x = GetSystemMetricsForDpi(SM_CXFRAME, dpi);
	m_borderThickness.y = GetSystemMetricsForDpi(SM_CYFRAME, dpi);
	m_borderPadding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

	SIZE title_bar_size = {0};
	constexpr int topAndBottomBorder = 2;
	HTHEME theme = OpenThemeData(GetHWND(), L"WINDOW");
	GetThemePartSize(theme, nullptr, WP_CAPTION, CS_ACTIVE, nullptr, TS_TRUE, &title_bar_size);
	CloseThemeData(theme);

	const int height = GetScaleUsingDpi(title_bar_size.cy, dpi) / 16 + topAndBottomBorder;

	RECT rect;
	::GetClientRect(GetHWND(), &rect);
	rect.bottom = rect.top + height;
	m_titleBarRect.SetTop(rect.top);
	m_titleBarRect.SetBottom(rect.bottom);
	m_titleBarRect.SetLeft(rect.left);
	m_titleBarRect.SetRight(rect.right);

	// Sadly SM_CXSIZE does not result in the right size buttons for Win10
	int buttonWidth = GetScaleUsingDpi(47, dpi) / 16;
	m_titleBarButtonRects.close = m_titleBarRect;
	m_titleBarButtonRects.close.SetTop(m_titleBarButtonRects.close.GetTop() + FAKE_SHADOW_HEIGHT);
	m_titleBarButtonRects.close.SetLeft(m_titleBarButtonRects.close.GetRight() - buttonWidth);
	m_titleBarButtonRects.close.SetWidth(buttonWidth);

	m_titleBarButtonRects.maximize = m_titleBarButtonRects.close;
	m_titleBarButtonRects.maximize.Offset(-buttonWidth, 0);
	
	m_titleBarButtonRects.minimize = m_titleBarButtonRects.maximize;
	m_titleBarButtonRects.minimize.Offset(-buttonWidth, 0);
}
