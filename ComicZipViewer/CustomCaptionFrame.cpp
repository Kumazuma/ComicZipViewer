#include "framework.h"
#include "CustomCaptionFrame.h"
#include <uxtheme.h>
#include <Vsstyle.h>
#include <vssym32.h>

#include "ComicZipViewer.h"
#include "tick.h"

inline static int GetScaleUsingDpi(int value, int dpi)
{
	return ((value * 16) * ((dpi * 16) / 96)) / 16;
}

class CustomCaptionFrame::ShowWithFadeInWorker: public UpdateSystem::Worker
{
public:
	ShowWithFadeInWorker(CustomCaptionFrame* pFrame)
		: m_pFrame(pFrame)
	{

	}

	void Update(uint64_t delta) override
	{
		m_pFrame->m_captionOpacity += delta * 20;
		if ( m_pFrame->m_captionOpacity >= 4096 )
		{
			m_pFrame->m_captionOpacity = 4096;
			m_pFrame->Render();
		}

		m_pFrame->TryRender();
	}

	bool IsFinished() override
	{
		return !m_pFrame->m_shownCaption
			|| m_pFrame->IsBeingDeleted()
			|| m_pFrame->m_captionOpacity >= 4096;
	}

private:

	CustomCaptionFrame* m_pFrame;
};

class CustomCaptionFrame::HideWithFadeOutWorker: public UpdateSystem::Worker
{
public:
	HideWithFadeOutWorker(CustomCaptionFrame* pFrame)
		: m_pFrame(pFrame)
	{

	}

	void Update(uint64_t delta) override
	{

		if ( m_pFrame->m_captionOpacity < delta * 20 )
		{
			m_pFrame->m_captionOpacity = 0;
			m_pFrame->Render();
			return;
		}

		m_pFrame->m_captionOpacity -= delta * 20;
		m_pFrame->TryRender();
	}

	bool IsFinished() override
	{
		return m_pFrame->m_shownCaption
			|| m_pFrame->IsBeingDeleted()
			|| m_pFrame->m_captionOpacity == 0;
	}

private:
	CustomCaptionFrame* m_pFrame;
};

CustomCaptionFrame::CustomCaptionFrame()
: m_currentHoveredButtonId(wxID_ANY)
, m_borderPadding()
, m_borderThickness()
, m_willRender()
, m_buttonDownedId(wxID_ANY)
, m_captionOpacity(0.f)
, m_shownCaption()
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


	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED , __uuidof( m_dwFactory ) , &m_dwFactory);
	UpdateCaptionDesc(dpi);
	m_renderSystem.Initalize(this, GetClientSize());

	auto rect = GetRect();
	SetWindowPos(
		GetHWND(), nullptr,
		rect.GetX(), rect.GetY(),
		rect.GetWidth(), rect.GetHeight(),
		SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOSENDCHANGING
	);
	return true;
}

WXLRESULT CustomCaptionFrame::MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam)
{
	switch(nMsg)
	{
	case WM_SIZE:
	{
		RECT rect;
		UpdateCaptionDesc(::GetDpiForWindow(GetHWND()));
		::GetClientRect(GetHWND(), &rect);
		m_renderSystem.ResizeSwapChain(wxSize{rect.right - rect.left, rect.bottom - rect.top});
		TryRender();
	}

		break;

	case WM_NCPAINT:
		return DefWindowProcW(GetHWND(), nMsg, wParam, lParam);

	case WM_NCCALCSIZE:
		return OnNcCalcSize(nMsg, wParam, lParam);

	case WM_NCHITTEST:
		return OnNcHitTest(nMsg, wParam, lParam);

	case WM_NCLBUTTONDOWN:
		return OnNcLButtonDown(nMsg, wParam, lParam);

	case WM_NCLBUTTONUP:
		return OnNcLButtonUp(nMsg, wParam, lParam);

	case WM_MOUSEMOVE:
	case WM_NCMOUSEMOVE:
		return OnNcMouseMove(nMsg, wParam, lParam);

	case WM_NCMOUSELEAVE:
		return OnNcMouseLeave(nMsg, wParam, lParam);

	case WM_NCMOUSEHOVER:
		return OnNcMouseEnter(nMsg, wParam, lParam);

	case WM_DPICHANGED:
		UpdateCaptionDesc(::GetDpiForWindow(GetHWND()));
		TryRender();
		break;

	case WM_SETTEXT:
		TryRender();
		break;
	}

	return wxFrame::MSWWindowProc(nMsg , wParam , lParam);
}

void CustomCaptionFrame::MSWUpdateFontOnDPIChange(const wxSize& newDPI)
{
	wxFrame::MSWUpdateFontOnDPIChange(newDPI);
	UpdateCaptionDesc(newDPI.x);
	TryRender();
}

inline D2D1_RECT_F GetRect(wxRect& rc)
{
	return D2D1::RectF(
		( float ) rc.GetLeft() ,
		( float ) rc.GetTop() ,
		( float ) rc.GetRight() ,
		( float ) rc.GetBottom()
	);
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
	if( m_captionOpacity != 0)
	{
		auto parameter = D2D1::LayerParameters(::GetRect(m_titleBarRect));
		parameter.contentBounds.right += 1.f;
		parameter.contentBounds.bottom += 1.f;
		parameter.opacity = m_captionOpacity * ( 1.f / 4096.f );
		d2dContext->PushLayer(parameter , nullptr);
		RenderCaption();
		d2dContext->PopLayer();
	}

	RenderButtons();
	
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

void CustomCaptionFrame::RenderCaption()
{
	ComPtr<ID2D1SolidColorBrush> lightGrayBrush;
	ComPtr<ID2D1SolidColorBrush> hoveredColorBrush;
	ComPtr<ID2D1SolidColorBrush> buttonColorBrush;
	ComPtr<ID2D1SolidColorBrush> whiteColorBrush;
	ComPtr<ID2D1SolidColorBrush> blackColorBrush;
	ComPtr<ID2D1DeviceContext1> context;
	D2D1::ColorF buttonColor(127.f / 255.f, 127.f / 255.f, 127.f / 255.f, 1.f);
	if(HasFocus())
	{
		buttonColor = D2D1::ColorF(33.f / 255.f, 33.f / 255.f, 33.f / 255.f, 1.f);
	}

	m_renderSystem.GetSolidColorBrush(D2D1::ColorF(130.f / 255.f, 130.f / 255.f, 130.f / 255.f, 1.f), &hoveredColorBrush);
	m_renderSystem.GetSolidColorBrush(buttonColor, &buttonColorBrush);
	m_renderSystem.GetSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &whiteColorBrush);
	m_renderSystem.GetSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightGray), &lightGrayBrush);
	m_renderSystem.GetSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &blackColorBrush);
	m_renderSystem.GetD2DDeviceContext(&context);
	// Paint Title Bar
	context->FillRectangle(D2D1::RectF(
		m_titleBarRect.GetLeft(),
		m_titleBarRect.GetTop(),
		m_titleBarRect.GetRight() + 1.f,
		m_titleBarRect.GetBottom() + 1.f
	), lightGrayBrush.Get());
	

	// Draw Text
	float leftButtonsStart = m_titleBarButtonRects.fullScreen.GetLeft();
	context->PushLayer(D2D1::LayerParameters(D2D1::RectF(0.f, 0.f, leftButtonsStart, m_titleBarRect.GetBottom() + 1.f)), nullptr);

	auto title = GetTitle();
	auto titleArea = ::GetRect(m_titleBarRect);
	titleArea.left += ( titleArea.bottom - titleArea.top );
	const float areaCenter = ( titleArea.bottom + titleArea.top ) * 0.5f;
	titleArea.top = areaCenter - m_titleTextSize * 0.5f;
	titleArea.bottom = areaCenter + m_titleTextSize * 0.5f;
	titleArea.right = std::numeric_limits<int16_t>::max();
	context->DrawText(title.wx_str() , title.Length() , m_dwTextFormat.Get() , titleArea , blackColorBrush.Get());

	context->PopLayer();
}

void CustomCaptionFrame::RenderButtons()
{
	ComPtr<ID2D1SolidColorBrush> lightGrayBrush;
	ComPtr<ID2D1SolidColorBrush> hoveredColorBrush;
	ComPtr<ID2D1SolidColorBrush> buttonColorBrush;
	ComPtr<ID2D1SolidColorBrush> whiteColorBrush;
	ComPtr<ID2D1SolidColorBrush> blackColorBrush;
	ComPtr<ID2D1DeviceContext1> context;
	D2D1::ColorF buttonColor(127.f / 255.f, 127.f / 255.f, 127.f / 255.f, 1.f);
	if(HasFocus())
	{
		buttonColor = D2D1::ColorF(33.f / 255.f, 33.f / 255.f, 33.f / 255.f, 1.f);
	}

	m_renderSystem.GetSolidColorBrush(D2D1::ColorF(130.f / 255.f, 130.f / 255.f, 130.f / 255.f, 1.f), &hoveredColorBrush);
	m_renderSystem.GetSolidColorBrush(buttonColor, &buttonColorBrush);
	m_renderSystem.GetSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &whiteColorBrush);
	m_renderSystem.GetSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightGray), &lightGrayBrush);
	m_renderSystem.GetSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black), &blackColorBrush);
	m_renderSystem.GetD2DDeviceContext(&context);
	context->FillRectangle(D2D1::RectF(
		m_titleBarButtonRects.fullScreen.GetLeft(),
		m_titleBarButtonRects.fullScreen.GetTop(),
		m_titleBarButtonRects.close.GetRight() + 1.f,
		m_titleBarButtonRects.close.GetBottom()
	), lightGrayBrush.Get());
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

	case wxID_FULLSCREEN_BOX:
		rc = &m_titleBarButtonRects.fullScreen;
	}

	if(rc != nullptr)
	{
		auto rcf = D2D1::RectF(
			(float)rc->GetLeft(),
			(float)rc->GetTop(),
			(float)rc->GetRight() + 1.f,
			(float)rc->GetBottom()
		);

		context->FillRectangle(rcf, hoveredColorBrush.Get());
	}

	const int dpi = ::GetDpiForWindow(GetHWND());
	float icon_dimension = GetScaleUsingDpi(10, dpi) / 16.f;
	D2D1_RECT_F icon_rect{ };
	if(!IsFullScreen())
	{
		icon_rect.right = icon_dimension;
		icon_rect.bottom = GetScaleUsingDpi(1 , dpi) / 16.f;
		win32_center_rect_in_rect(&icon_rect , ::GetRect(m_titleBarButtonRects.minimize));
		context->FillRectangle(icon_rect , buttonColorBrush.Get());

		icon_rect.top = 0.f;
		icon_rect.left = 0.f;
		icon_rect.right = icon_dimension;
		icon_rect.bottom = icon_dimension;
		win32_center_rect_in_rect(&icon_rect , ::GetRect(m_titleBarButtonRects.maximize));
		if ( IsMaximized() )
		{
			auto rc = icon_rect;
			rc.left += GetScaleUsingDpi(MAXIMIZED_RECTANGLE_OFFSET , dpi) / 16.f;
			rc.top -= GetScaleUsingDpi(MAXIMIZED_RECTANGLE_OFFSET , dpi) / 16.f;
			rc.right += GetScaleUsingDpi(MAXIMIZED_RECTANGLE_OFFSET , dpi) / 16.f;
			rc.bottom -= GetScaleUsingDpi(MAXIMIZED_RECTANGLE_OFFSET , dpi) / 16.f;
			context->DrawRectangle(rc , buttonColorBrush.Get() , GetScaleUsingDpi(1 , dpi) / 16.f);

			rc = icon_rect;
			rc.left -= GetScaleUsingDpi(MAXIMIZED_RECTANGLE_OFFSET , dpi) / 16.f;
			rc.top += GetScaleUsingDpi(MAXIMIZED_RECTANGLE_OFFSET , dpi) / 16.f;
			rc.right -= GetScaleUsingDpi(MAXIMIZED_RECTANGLE_OFFSET , dpi) / 16.f;
			rc.bottom += GetScaleUsingDpi(MAXIMIZED_RECTANGLE_OFFSET , dpi) / 16.f;
			ID2D1SolidColorBrush* bgColorBrush = lightGrayBrush.Get();
			if ( m_currentHoveredButtonId == wxID_MAXIMIZE_BOX )
			{
				bgColorBrush = hoveredColorBrush.Get();
			}

			context->FillRectangle(rc , bgColorBrush);
			context->DrawRectangle(rc , buttonColorBrush.Get() , GetScaleUsingDpi(1 , dpi) / 16.f);
		}
		else
		{
			context->DrawRectangle(icon_rect , buttonColorBrush.Get() , GetScaleUsingDpi(1, dpi) / 16.f);
		}
	}
	
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

	context->DrawLine(D2D1::Point2F(icon_rect.left, icon_rect.top), D2D1::Point2F(icon_rect.right, icon_rect.bottom), pBrush, GetScaleUsingDpi(1 , dpi) / 16.f);
	context->DrawLine(D2D1::Point2F(icon_rect.left, icon_rect.bottom), D2D1::Point2F(icon_rect.right, icon_rect.top), pBrush, GetScaleUsingDpi(1 , dpi) / 16.f);

	icon_rect.top = 0.f;
	icon_rect.left = 0.f;
	icon_rect.right = icon_dimension + GetScaleUsingDpi(4 , dpi) / 16.f;
	icon_rect.bottom = icon_dimension + GetScaleUsingDpi(4 , dpi) / 16.f;
	win32_center_rect_in_rect(&icon_rect , ::GetRect(m_titleBarButtonRects.fullScreen));
	context->DrawRoundedRectangle(D2D1::RoundedRect(icon_rect , GetScaleUsingDpi(1 , dpi) / 16.f , GetScaleUsingDpi(1 , dpi) / 16.f), buttonColorBrush.Get(), GetScaleUsingDpi(1 , dpi) / 16.f);
	
	{
		ID2D1SolidColorBrush* bgColorBrush = lightGrayBrush.Get();
		if ( m_currentHoveredButtonId == wxID_FULLSCREEN_BOX )
		{
			bgColorBrush = hoveredColorBrush.Get();
		}

	//	auto rect = icon_rect;
	//	rect.top += GetScaleUsingDpi(1 , dpi) / 16.f;
	//	rect.left += GetScaleUsingDpi(1 , dpi) / 16.f;
	//	rect.right -= GetScaleUsingDpi(1 , dpi) / 16.f;
	//	rect.bottom -= GetScaleUsingDpi(1 , dpi) / 16.f;
	//	context->FillRoundedRectangle(D2D1::RoundedRect(rect , GetScaleUsingDpi(1 , dpi) / 16.f , GetScaleUsingDpi(1 , dpi) / 16.f), bgColorBrush);

		icon_rect.left -= 1;
		icon_rect.top -= 1;
		icon_rect.bottom += 1;
		icon_rect.right += 1;
		auto rect2 = icon_rect;
		rect2.top += GetScaleUsingDpi(5 , dpi) / 16.f;
		rect2.bottom -= GetScaleUsingDpi(5 , dpi) / 16.f;
		context->FillRectangle(rect2 , bgColorBrush);

		rect2 = icon_rect;
		rect2.left += GetScaleUsingDpi(5 , dpi) / 16.f;
		rect2.right -= GetScaleUsingDpi(5 , dpi) / 16.f;
		context->FillRectangle(rect2 , bgColorBrush);

		rect2 = icon_rect;
		rect2.top += GetScaleUsingDpi(5 , dpi) / 16.f;
		rect2.left += GetScaleUsingDpi(5 , dpi) / 16.f;
		rect2.right -= GetScaleUsingDpi(5 , dpi) / 16.f;
		rect2.bottom -= GetScaleUsingDpi(5 , dpi) / 16.f;
		context->DrawRoundedRectangle(D2D1::RoundedRect(rect2, GetScaleUsingDpi(1 , dpi) / 16.f, GetScaleUsingDpi(1 , dpi) / 16.f) , buttonColorBrush.Get(), GetScaleUsingDpi(1 , dpi) / 16.f);
	}

}

void CustomCaptionFrame::DoThaw()
{
	wxFrame::DoThaw();
	Render();
}

void CustomCaptionFrame::ResizeSwapChain(wxSize& size)
{
	m_renderSystem.ResizeSwapChain(size);
}

HRESULT CustomCaptionFrame::GetD2DDeviceContext(ID2D1DeviceContext1** ppContext)
{
	return m_renderSystem.GetD2DDeviceContext(ppContext);
}

HRESULT CustomCaptionFrame::GetD3DDeviceContext(ID3D11DeviceContext** ppContext)
{
	return m_renderSystem.GetD3DDeviceContext(ppContext);
}

HRESULT CustomCaptionFrame::GetD3DDevice(ID3D11Device** ppDevice)
{
	return m_renderSystem.GetD3DDevice(ppDevice);
}

WXLRESULT CustomCaptionFrame::OnNcCalcSize(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	if(!wParam)
	{
		return wxFrame::MSWWindowProc(nMsg , wParam , lParam);
	}

	NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lParam;
	RECT* requestedClientRect = params->rgrc;

	if(!IsFullScreen())
	{
		requestedClientRect->right -= m_borderThickness.x;
		requestedClientRect->left += m_borderThickness.x;
		requestedClientRect->bottom -= m_borderThickness.y;
		if (IsMaximized()) 
		{
			requestedClientRect->right += m_borderThickness.x;
			requestedClientRect->left -= m_borderThickness.x;
			requestedClientRect->bottom += m_borderThickness.y;
			requestedClientRect->top += m_borderPadding;
		}
	}

	return 0;
}

WXLRESULT CustomCaptionFrame::OnNcHitTest(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	WXLRESULT hitTestRet = wxFrame::MSWWindowProc(nMsg, wParam, lParam);
	// 10 11 12 13 14 15 16 17
	if(hitTestRet == HTNOWHERE
		// HTLEFT HTRIGHT HTTOP HTTBOTTOM HTTOPLEFT HTTOPRIGHT HTBOTTOMLEFT HTBOTTOMRIGHT
		|| (HTLEFT <= hitTestRet && hitTestRet <= HTBOTTOMRIGHT))
	{
			return hitTestRet;
	}

	const bool isFullScreen = IsFullScreen();

	if ( !isFullScreen )
	{
		// Check if hover button is on maximize to support SnapLayout on Windows 11
		if ( m_currentHoveredButtonId == wxID_MAXIMIZE_BOX )
		{
			return HTMAXBUTTON;
		}

		if ( m_currentHoveredButtonId == wxID_MINIMIZE_BOX )
		{
			return HTMINBUTTON;
		}
	}

	if(m_currentHoveredButtonId == wxID_CLOSE_BOX)
	{
		return HTCLOSE;
	}
	else if(m_currentHoveredButtonId == wxID_FULLSCREEN_BOX )
	{
		return HTCAPTION;
	}

	// Looks like adjustment happening in NCCALCSIZE is messing with the detection
	// of the top hit area so manually fixing that.
	wxPoint cursorPoint(LOWORD(lParam), HIWORD(lParam));
	cursorPoint = ScreenToClient(cursorPoint);
	if (cursorPoint.y > 0
		&& cursorPoint.y < m_borderThickness.y + m_borderPadding)
	{
		return HTTOP;
	}

	// Since we are drawing our own caption, this needs to be a custom test
	if ( cursorPoint.y < m_titleBarRect.GetBottom() )
	{
		if ( !m_shownCaption )
		{
			m_shownCaption = true;
			wxGetApp().GetUpdateSystem().AddWorker(new ShowWithFadeInWorker(this));
		}

		if( !isFullScreen )
		{
			return HTCAPTION;
		}
	}
	else if ( m_shownCaption )
	{
		m_shownCaption = false;
		wxGetApp().GetUpdateSystem().AddWorker(new HideWithFadeOutWorker(this));
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
	{
		m_buttonDownedId = m_currentHoveredButtonId;
		return 0;
	}

	return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
}

WXLRESULT CustomCaptionFrame::OnNcLButtonUp(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	if(m_buttonDownedId != m_currentHoveredButtonId)
	{
		return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
	}

	m_buttonDownedId = wxID_ANY;
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

	case wxID_FULLSCREEN_BOX:
		ShowFullScreen(!IsFullScreen());
		return 0;
	}

	return wxFrame::MSWWindowProc(nMsg, wParam, lParam);
}

WXLRESULT CustomCaptionFrame::OnNcMouseMove(UINT nMsg, WPARAM wParam, LPARAM lParam)
{
	wxPoint cursor = wxGetMousePosition();
	ScreenToClient(&cursor.x, &cursor.y);
	wxWindowID id = GetMouseOveredButtonId(cursor);
	if(m_currentHoveredButtonId != id)
	{
		TryRender();
	}

	m_currentHoveredButtonId = id;
	if ( cursor.y <= m_titleBarRect.GetBottom() )
	{
		if( !m_shownCaption )
		{
			m_shownCaption = true;
			wxGetApp().GetUpdateSystem().AddWorker(new ShowWithFadeInWorker(this));
		}
	}
	else if( m_shownCaption )
	{
		m_shownCaption = false;
		wxGetApp().GetUpdateSystem().AddWorker(new HideWithFadeOutWorker(this));
	}

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
	m_borderPadding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, 96);

	SIZE title_bar_size = {0};
	LOGFONTW fontDesc;
	constexpr int topAndBottomBorder = 2;
	HTHEME theme = OpenThemeData(GetHWND(), L"WINDOW");
	GetThemePartSize(theme, nullptr, WP_CAPTION, CS_ACTIVE, nullptr, TS_TRUE, &title_bar_size);
	GetThemeSysFont(theme , TMT_CAPTIONFONT , &fontDesc);
	CloseThemeData(theme);

	const int height = GetScaleUsingDpi(title_bar_size.cy + topAndBottomBorder, dpi) / 16;
	m_titleTextSize = GetScaleUsingDpi(fontDesc.lfHeight , dpi) * ( 1.f / 16.f );
	if(fontDesc.lfHeight < 0)
	{
		m_titleTextSize = -(fontDesc.lfHeight / (GetDpiForSystem() / 96.f)) * dpi / 96.f;
	}

	m_dwFactory->CreateTextFormat(fontDesc.lfFaceName , nullptr , ( DWRITE_FONT_WEIGHT ) fontDesc.lfWeight , DWRITE_FONT_STYLE_NORMAL , DWRITE_FONT_STRETCH_NORMAL , m_titleTextSize, L"ko-KR" , &m_dwTextFormat);
	
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
	m_titleBarButtonRects.maximize.Offset(-buttonWidth , 0);

	m_titleBarButtonRects.minimize = m_titleBarButtonRects.maximize;
	m_titleBarButtonRects.minimize.Offset(-buttonWidth , 0);

	if(IsFullScreen())
	{
		m_titleBarButtonRects.fullScreen = m_titleBarButtonRects.close;
	}
	else
	{
		m_titleBarButtonRects.fullScreen = m_titleBarButtonRects.minimize;
	}

	m_titleBarButtonRects.fullScreen.Offset(-buttonWidth , 0);
}

wxWindowID CustomCaptionFrame::GetMouseOveredButtonId(const wxPoint& cursor) const
{
	wxWindowID id = wxID_ANY;
	if (m_titleBarButtonRects.fullScreen.GetX() <= cursor.x
		&& m_titleBarButtonRects.fullScreen.GetBottom() >= cursor.y)
	{
		if (m_titleBarButtonRects.close.Contains(cursor))
		{
			id = wxID_CLOSE_BOX;
		}
		else if( !IsFullScreen() )
		{
			if ( m_titleBarButtonRects.minimize.Contains(cursor) )
			{
				id = wxID_MINIMIZE_BOX;
			}
			else if ( m_titleBarButtonRects.maximize.Contains(cursor) )
			{
				id = wxID_MAXIMIZE_BOX;
			}
		}

		if(m_titleBarButtonRects.fullScreen.Contains(cursor))
		{
			id = wxID_FULLSCREEN_BOX;
		}
	}

	return id;
}

HRESULT CustomCaptionFrame::GetContainingOutput(IDXGIOutput** ppOutput)
{
	ComPtr<IDXGISwapChain1> swapChain;
	m_renderSystem.GetSwapChain(&swapChain);
	return swapChain->GetContainingOutput(ppOutput);
}
