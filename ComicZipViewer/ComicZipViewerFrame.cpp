#include "framework.h"
#include "ComicZipViewerFrame.h"
#include "ComicZipViewer.h"
#include "CsRgb24ToRgba32"
#include "CsRgb24WithAlphaToRgba32"
#include <wx/bitmap.h>

#include "tick.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "sqlite.lib")
#pragma comment(lib, "windowscodecs.lib")

constexpr float SEEK_BAR_PADDING = 15.f;
constexpr float SEEK_BAR_TRACK_HEIGHT = 5.f;
constexpr float SEEK_BAR_THUMB_RADIUS = 10.f;
constexpr std::wstring_view ID_SVG_FIT_PAGE = wxS("ID_SVG_FIT_PAGE");
constexpr std::wstring_view ID_SVG_FIT_WIDTH = wxS("ID_SVG_FIT_WIDTH");
constexpr std::wstring_view ID_SVG_PAGE = wxS("ID_SVG_PAGE");
constexpr std::wstring_view ID_SVG_MARKED_PAGE = wxS("ID_SVG_MARKED_PAGE");
constexpr std::wstring_view ID_SVG_UNMARKED_PAGE = wxS("ID_SVG_UNMARKED_PAGE");
constexpr std::wstring_view ID_SVG_TEXT_BULLET_LIST_SQUARE = wxS("ID_SVG_TEXT_BULLET_LIST_SQUARE");
constexpr std::wstring_view ID_SVG_MOVE_TO_NEXT_BOOK = wxS("ID_SVG_MOVE_TO_NEXT_BOOK");
constexpr std::wstring_view ID_SVG_MOVE_TO_PREV_BOOK = wxS("ID_SVG_MOVE_TO_PREV_BOOK");

class ComicZipViewerFrame::ShowWithFadeInWorker: public UpdateSystem::Worker
{
public:
	ShowWithFadeInWorker(ComicZipViewerFrame* pFrame)
		: m_pFrame(pFrame)
	{

	}

	void Update(uint64_t delta) override
	{
		m_pFrame->m_alphaControlPanel += delta * ( 1.f / 4096.f ) * 20.f;
		if ( m_pFrame->m_alphaControlPanel >= 1.f )
		{
			m_pFrame->m_alphaControlPanel = 1.f;
		}


		m_pFrame->TryRender();
	}

	bool IsFinished() override
	{
		return !m_pFrame->m_shownControlPanel
			|| m_pFrame->IsBeingDeleted()
			|| m_pFrame->m_alphaControlPanel >= 1.f;
	}

private:
	ComicZipViewerFrame* m_pFrame;
};

class ComicZipViewerFrame::HideWithFadeOutWorker: public UpdateSystem::Worker
{
public:
	HideWithFadeOutWorker(ComicZipViewerFrame* pFrame)
		: m_pFrame(pFrame)
	{

	}

	void Update(uint64_t delta) override
	{
		m_pFrame->m_alphaControlPanel -= delta * ( 1.f / 4096.f ) * 20.f;
		if ( m_pFrame->m_alphaControlPanel <= 0.f )
		{
			m_pFrame->m_alphaControlPanel = 0.f;
		}


		m_pFrame->TryRender();
	}

	bool IsFinished() override
	{
		return m_pFrame->m_shownControlPanel
			|| m_pFrame->IsBeingDeleted()
			|| m_pFrame->m_alphaControlPanel <= 0.f;
	}

private:
	ComicZipViewerFrame* m_pFrame;
};

ComicZipViewerFrame::ComicZipViewerFrame()
: m_enterIsDown(false)
, m_pContextMenu(nullptr)
, m_alphaControlPanel(0.f)
, m_shownControlPanel(false)
, m_offsetSeekbarThumbPos()
, m_valueSeekBar()
, m_willRender(false)
, m_imageViewMode()
, m_latestHittenButtonId(wxID_ANY)
, m_idMouseOver(wxID_ANY)
, m_pageIsMarked(false)
, m_toastSystem(this)
{
}

ComicZipViewerFrame::~ComicZipViewerFrame()
{
	DeletePendingEvents();
	ClearEventHashTable();
	delete m_pContextMenu;
}

bool ComicZipViewerFrame::Create(wxEvtHandler* pView)
{
	m_pView = pView;
	auto ret = CustomCaptionFrame::Create(nullptr, wxID_ANY, wxS("ComicZipViewer"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS | wxTINY_CAPTION);
	if ( !ret )
	{
		return false;
	}

	EnableTouchEvents(wxTOUCH_PAN_GESTURES);
	m_pContextMenu = new wxMenu();
	m_pContextMenu->Append(wxID_OPEN, wxS("Open(&O)"));


	m_pRecentFileMenu = new wxMenu();
	auto* pImageSizingModeMenu = new wxMenu();
	pImageSizingModeMenu->Append(ID_BTN_ORIGINAL , wxS("original size"));
	pImageSizingModeMenu->Append(ID_BTN_FIT_WIDTH, wxS("Fit window's width"));
	pImageSizingModeMenu->Append(ID_BTN_FIT_PAGE , wxS("Fit window's area"));
	m_pContextMenu->AppendSubMenu(pImageSizingModeMenu , wxS("View modes"));
	m_pContextMenu->AppendSubMenu(m_pRecentFileMenu, wxS("Recent files"));
	m_pContextMenu->Append(wxID_CLOSE , wxS("Close(&C)"));

	wxBitmapBundle a;
	HRESULT hRet;
	ComPtr<ID2D1DeviceContext1> d2dContext;
	GetD2DDeviceContext(&d2dContext);
	hRet = d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.75f), &m_d2dBlackBrush);
	hRet = d2dContext->CreateSolidColorBrush(D2D1::ColorF( 0.f, 0.47f , 0.83f) , &m_d2dBlueBrush);
	hRet = d2dContext->CreateSolidColorBrush(D2D1::ColorF(1.f , 1.f , 1.f) , &m_d2dWhiteBrush);
	hRet = d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.7f , 0.7f , 0.7f, 0.75f) , &m_d2dBackGroundWhiteBrush);
	hRet = d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.2f , 0.2f , 0.2f) , &m_d2dGrayBrush);
	hRet = d2dContext->CreateLayer(&m_controlPanelLayer);

	m_iconBitmapInfo.emplace(ID_SVG_FIT_PAGE, std::tuple<wxBitmapBundle, D2D1_RECT_F>{});
	m_iconBitmapInfo.emplace(ID_SVG_FIT_WIDTH, std::tuple<wxBitmapBundle, D2D1_RECT_F>{});
	m_iconBitmapInfo.emplace(ID_SVG_PAGE, std::tuple<wxBitmapBundle, D2D1_RECT_F>{});
	m_iconBitmapInfo.emplace(ID_SVG_MARKED_PAGE, std::tuple<wxBitmapBundle, D2D1_RECT_F>{});
	m_iconBitmapInfo.emplace(ID_SVG_TEXT_BULLET_LIST_SQUARE, std::tuple<wxBitmapBundle, D2D1_RECT_F>{});
	m_iconBitmapInfo.emplace(ID_SVG_UNMARKED_PAGE, std::tuple<wxBitmapBundle, D2D1_RECT_F>{});
	m_iconBitmapInfo.emplace(ID_SVG_MOVE_TO_PREV_BOOK, std::tuple<wxBitmapBundle, D2D1_RECT_F>{});
	m_iconBitmapInfo.emplace(ID_SVG_MOVE_TO_NEXT_BOOK, std::tuple<wxBitmapBundle, D2D1_RECT_F>{});
	for(auto& pair: m_iconBitmapInfo)
	{
		auto& bundle = std::get<0>(pair.second);
		bundle = wxBitmapBundle::FromSVGResource(pair.first.data(), wxSize(64, 64));
	}

	GenerateIconBitmaps();

	auto font = GetFont();
	font.SetFractionalPointSize(font.GetFractionalPointSize() * 4);
	font.SetWeight(wxFONTWEIGHT_HEAVY);

	m_toastSystem.SetFont(font);
	m_toastSystem.SetColor(d2dContext, D2D1::ColorF(D2D1::ColorF::White), D2D1::ColorF(D2D1::ColorF(D2D1::ColorF::Black)));
	UpdateClientSize(GetClientSize());

	return true;
}

void ComicZipViewerFrame::ShowImage(const ComPtr<IWICBitmap>& image)
{
	HRESULT hr;
	ComPtr<ID2D1Bitmap1> bitmap;
	ComPtr<ID3D11Device> d3dDevice;
	ComPtr<ID2D1DeviceContext1> d2dContext;
	ComPtr<ID3D11DeviceContext> d3dContext;
	GetD2DDeviceContext(&d2dContext);
	GetD3DDeviceContext(&d3dContext);
	GetD3DDevice(&d3dDevice);
	if(!image)
	{
		ComPtr<ID2D1Image> oldTarget;
		d2dContext->GetTarget(&oldTarget);
		d2dContext->CreateBitmap(D2D1::SizeU(512, 512), nullptr, 0, D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_GDI_COMPATIBLE,
			D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);
		d2dContext->SetTarget(bitmap.Get());
		ComPtr<ID2D1GdiInteropRenderTarget> renderTarget;
		hr = d2dContext.As(&renderTarget);
		HDC hDC;
		SIZE size;
		d2dContext->BeginDraw();
		renderTarget->GetDC(D2D1_DC_INITIALIZE_MODE_CLEAR, &hDC);
		::Rectangle(hDC, 0, 0, 512, 512);
		::MoveToEx(hDC, 0, 0, nullptr);
		::LineTo(hDC, 512, 512);
		::MoveToEx(hDC, 512, 0, nullptr);
		::LineTo(hDC, 0, 512);
		::GetTextExtentPointW(hDC, L"Cannot open image file", sizeof(L"Cannot open image file") / sizeof(wchar_t) - 1, &size);
		TextOutW(hDC, (512 - size.cx) >> 1, (512 - size.cy) >> 1, L"Cannot open image file", sizeof(L"Cannot open image file") / sizeof(wchar_t) - 1);
		renderTarget->ReleaseDC(nullptr);
		renderTarget.Reset();
		d2dContext->EndDraw();
		d2dContext->SetTarget(oldTarget.Get());
		m_bitmap = bitmap;
		m_imageSize = wxSize(512, 512);
		UpdateScaledImageSize();
		TryRender();
		return;
	}

	D3D11_TEXTURE2D_DESC texture2dDesc{};
	WICPixelFormatGUID pixelFormat;
	image->GetPixelFormat(&pixelFormat);
	bool hasAlpha = pixelFormat == GUID_WICPixelFormat32bppRGBA;
	const D2D1_ALPHA_MODE alphaMode = hasAlpha ? D2D1_ALPHA_MODE_PREMULTIPLIED : D2D1_ALPHA_MODE_IGNORE;
	UINT width;
	UINT height;
	image->GetSize(&width, &height);
	bool needCreationTexture = m_bitmap == nullptr || m_d3dTexture2d == nullptr || (width != m_imageSize.GetWidth() || height != m_imageSize.GetHeight());
	bool needCreationBitmap = needCreationTexture;
	if(!needCreationBitmap)
	{
		needCreationBitmap = m_bitmap->GetPixelFormat().alphaMode != alphaMode;
	}
	
	if(needCreationBitmap)
	{
		m_bitmap.Reset();
	}

	if(needCreationTexture)
	{
		m_d3dTexture2d.Reset();
		m_d3dUavTexture2d.Reset();

		texture2dDesc.ArraySize = 1;
		texture2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texture2dDesc.MipLevels = 1;
		texture2dDesc.CPUAccessFlags = 0;
		texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
		texture2dDesc.SampleDesc.Count = 1;
		texture2dDesc.Width = width;
		texture2dDesc.Height = height;
		hr = d3dDevice->CreateTexture2D(&texture2dDesc, nullptr, &m_d3dTexture2d);

		hr = d3dDevice->CreateUnorderedAccessView(m_d3dTexture2d.Get() , nullptr , &m_d3dUavTexture2d);
		assert(SUCCEEDED(hr));
	}
	else
	{
		bitmap = m_bitmap;
	}

	ComPtr<ID3D11Texture2D> srcTexture;
	texture2dDesc.ArraySize = 1;
	texture2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texture2dDesc.MipLevels = 1;
	texture2dDesc.CPUAccessFlags = 0;
	texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
	texture2dDesc.SampleDesc.Count = 1;
	WICRect rect{0, 0, (int)width, (int)height};
	ComPtr<IWICBitmapLock> locker;
	hr = image->Lock(&rect, WICBitmapLockRead, &locker);
	UINT bufferSize;
	UINT stride;
	BYTE* ptr;
	D3D11_SUBRESOURCE_DATA subResData{};
	locker->GetDataPointer(&bufferSize, &ptr);
	locker->GetStride(&stride);
	subResData.pSysMem = ptr;
	subResData.SysMemPitch = stride;
	texture2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texture2dDesc.Width = width;
	texture2dDesc.Height = height;
	hr = d3dDevice->CreateTexture2D(&texture2dDesc , &subResData , &srcTexture);
	assert(SUCCEEDED(hr));
	d3dContext->CopyResource(m_d3dTexture2d.Get(), srcTexture.Get());
	
	if(needCreationBitmap)
	{
		ComPtr<IDXGISurface> surface;
		hr = m_d3dTexture2d.As(&surface);
		hr = d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, alphaMode)), &bitmap);
		if(FAILED(hr))
		{
			wxLogError(wxS("Failed to create D2D Bitmap"));
			return;
		}
	}

	m_bitmap = bitmap;
	m_imageSize = wxSize(width, height);
	UpdateScaledImageSize();
	TryRender();
}

void ComicZipViewerFrame::OnKeyDown(wxKeyEvent& evt)
{
	wxCommandEvent event(wxEVT_BUTTON, wxID_ANY);
	event.SetEventObject(this);
	switch(evt.GetKeyCode())
	{
	case WXK_UP:
		ScrollImageVertical(m_clientSize.y * 0.04f, true);
		return;
	case WXK_DOWN:
		ScrollImageVertical(m_clientSize.y * -0.04f, true);
		return;

	case WXK_LEFT:
		event.SetId(wxID_BACKWARD);
		m_pView->ProcessEvent(event);
		return;
	case WXK_RIGHT:
		event.SetId(wxID_FORWARD);
		m_pView->ProcessEvent(event);
		return;
	}

	if(evt.GetKeyCode() == WXK_RETURN && evt.AltDown() && !m_enterIsDown)
	{
		m_enterIsDown = true;
		if(!IsFullScreen())
		{
			Fullscreen();
		}
		else
		{
			RestoreFullscreen();
		}
	}
}

void ComicZipViewerFrame::OnKeyUp(wxKeyEvent& evt)
{
	if(evt.GetKeyCode() == WXK_RETURN)
		m_enterIsDown = false;
}

void ComicZipViewerFrame::DoRender()
{
	ComPtr<ID2D1DeviceContext1> d2dContext;
	GetD2DDeviceContext(&d2dContext);
	if(m_bitmap)
	{
		d2dContext->SetTransform(D2D1::Matrix3x2F::Translation(m_clientSize.x * 0.5f , m_clientSize.y * 0.5f) * D2D1::Matrix3x2F::Translation(m_center.x, m_center.y));
		d2dContext->DrawBitmap(m_bitmap.Get() , D2D1::RectF(-m_scaledImageSize.width , -m_scaledImageSize.height , m_scaledImageSize.width , m_scaledImageSize.height), 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
		d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
	}

	if ( m_alphaControlPanel > 0.f )
	{
		const float scale = GetDPIScaleFactor();
		const auto& size = m_clientSize;
		const Rect& seekBarRect = m_seekBarRect;
		const float seekBarX = m_panelRect.left + SEEK_BAR_PADDING * scale;
		const float seekBarWidth = m_panelRect.GetWidth() - SEEK_BAR_PADDING * 2 * scale;
		d2dContext->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect() , nullptr , D2D1_ANTIALIAS_MODE_PER_PRIMITIVE , D2D1::IdentityMatrix() , m_alphaControlPanel) , m_controlPanelLayer.Get());
		d2dContext->FillRectangle(m_panelRect , m_d2dBackGroundWhiteBrush.Get());
		d2dContext->FillRoundedRectangle(D2D1::RoundedRect(seekBarRect , SEEK_BAR_TRACK_HEIGHT * 0.5f * scale , SEEK_BAR_TRACK_HEIGHT * 0.5f * scale) , m_d2dGrayBrush.Get());
		if ( wxGetApp().GetPageCount() >= 1 )
		{
			const float percent = m_valueSeekBar / ( float ) ( wxGetApp().GetPageCount() - 1 );
			const float bar = seekBarRect.GetWidth() * percent + SEEK_BAR_TRACK_HEIGHT * 0.5f * scale + seekBarRect.left;
			const float thumbCenterY = seekBarRect.top + SEEK_BAR_TRACK_HEIGHT * 0.5f * scale;
			Rect valueRect = seekBarRect;
			valueRect.right = bar;
			d2dContext->FillRoundedRectangle(D2D1::RoundedRect(valueRect , SEEK_BAR_TRACK_HEIGHT * 0.5f * scale , SEEK_BAR_TRACK_HEIGHT * 0.5f * scale) , m_d2dBlueBrush.Get());
			d2dContext->FillEllipse(D2D1::Ellipse(D2D1::Point2F(bar , thumbCenterY) , SEEK_BAR_THUMB_RADIUS * scale , SEEK_BAR_THUMB_RADIUS * scale) , m_d2dWhiteBrush.Get());
			d2dContext->FillEllipse(D2D1::Ellipse(D2D1::Point2F(bar , thumbCenterY) , 5.0f * scale , 5.0f * scale) , m_d2dBlueBrush.Get());
			d2dContext->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(bar , thumbCenterY) , SEEK_BAR_THUMB_RADIUS * scale , SEEK_BAR_THUMB_RADIUS * scale) , m_d2dBlueBrush.Get() , 0.5f , m_d2dSimpleStrokeStyle.Get());
		}

		if ( m_imageViewMode == ImageViewModeKind::ORIGINAL )
			d2dContext->FillRectangle(m_originalBtnRect , m_d2dWhiteBrush.Get());

		d2dContext->DrawBitmap(m_iconAtlas.Get() , m_originalBtnRect , 1.f , D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ ID_SVG_PAGE ]));

		if ( m_imageViewMode == ImageViewModeKind::FIT_PAGE )
			d2dContext->FillRectangle(m_fitPageBtnRect , m_d2dWhiteBrush.Get());

		d2dContext->DrawBitmap(m_iconAtlas.Get() , m_fitPageBtnRect , 1.f , D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ ID_SVG_FIT_PAGE ]));

		if ( m_imageViewMode == ImageViewModeKind::FIT_WIDTH )
			d2dContext->FillRectangle(m_fitWidthBtnRect , m_d2dWhiteBrush.Get());

		d2dContext->DrawBitmap(m_iconAtlas.Get() , m_fitWidthBtnRect , 1.f , D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ ID_SVG_FIT_WIDTH ]));

		if ( m_idMouseOver != wxID_ANY )
		{
			const D2D1_RECT_F* rc = nullptr;
			switch( m_idMouseOver )
			{
			case ID_BTN_BOOKMARK_VIEW:
				rc = &m_bookmarkViewBtnRect;
				break;

			case ID_BTN_ADD_MARK:
				rc = &m_addMarkBtnRect;
				break;

			case ID_BTN_MOVE_TO_NEXT_PAGE:
				rc = &m_moveToNextBookBtnRect;
				break;

			case ID_BTN_MOVE_TO_PREV_PAGE:
				rc = &m_moveToPrevBookBtnRect;
				break;
			}

			d2dContext->FillRectangle(*rc , m_d2dWhiteBrush.Get());
		}

		d2dContext->DrawBitmap(m_iconAtlas.Get(), m_bookmarkViewBtnRect, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, std::get<1>(m_iconBitmapInfo[ID_SVG_TEXT_BULLET_LIST_SQUARE]));
		d2dContext->DrawBitmap(m_iconAtlas.Get(), m_moveToPrevBookBtnRect, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ ID_SVG_MOVE_TO_PREV_BOOK ]));
		d2dContext->DrawBitmap(m_iconAtlas.Get() , m_moveToNextBookBtnRect , 1.f , D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ ID_SVG_MOVE_TO_NEXT_BOOK ]));

		if(m_pageIsMarked)
		{
			d2dContext->DrawBitmap(m_iconAtlas.Get(), m_addMarkBtnRect, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ID_SVG_MARKED_PAGE]));
		}
		else
		{
			d2dContext->DrawBitmap(m_iconAtlas.Get(), m_addMarkBtnRect, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ID_SVG_UNMARKED_PAGE]));
		}


		d2dContext->PopLayer();
	}

	m_toastSystem.Render(d2dContext);
}

void ComicZipViewerFrame::OnSize(wxSizeEvent& evt)
{
	UpdateClientSize(GetClientSize());
	Render();
}

void ComicZipViewerFrame::ShowToast(const wxString& text, bool preventSameToast)
{
	m_toastSystem.AddMessage(text, wxID_ANY, preventSameToast);
}

void ComicZipViewerFrame::Fullscreen()
{
	ShowFullScreen(true);
	ComPtr<IDXGIOutput> output;
	
	GetContainingOutput(&output);
	DXGI_OUTPUT_DESC desc{};
	output->GetDesc(&desc);
	const RECT& coordinates = desc.DesktopCoordinates;
	UpdateClientSize(wxSize(coordinates.right - coordinates.left , coordinates.bottom - coordinates.top));
	ResizeSwapChain(m_clientSize);
	Render();
}

void ComicZipViewerFrame::RestoreFullscreen()
{
	ShowFullScreen(false);
	UpdateClientSize(GetClientSize());
	ResizeSwapChain(m_clientSize);
	Render();
}

void ComicZipViewerFrame::OnMouseLeave(wxMouseEvent& evt)
{
}

void ComicZipViewerFrame::OnMouseMove(wxMouseEvent& evt)
{
	auto pos = evt.GetPosition();
	if(evt.Dragging() && m_prevMousePosition.has_value())
	{
		wxPoint diff = pos - *m_prevMousePosition;
		Freeze();
		if(diff.x != 0)
		{
			m_prevMousePosition->x = pos.x;
			ScrollImageHorizontal(diff.x, false);
		}

		if(diff.y != 0)
		{
			m_prevMousePosition->y = pos.y;
			ScrollImageVertical(diff.y, false);
		}
		Thaw();
	}

	const wxWindowID prevIdMouseOver = m_idMouseOver;
	m_idMouseOver = wxID_ANY;
	if(m_bookmarkViewBtnRect.Contain(pos))
	{
		m_idMouseOver = ID_BTN_BOOKMARK_VIEW;
	}
	else if(m_addMarkBtnRect.Contain(pos) )
	{
		m_idMouseOver = ID_BTN_ADD_MARK;
	}
	else if(m_moveToNextBookBtnRect.Contain(pos))
	{
		m_idMouseOver = ID_BTN_MOVE_TO_NEXT_PAGE;
	}
	else if ( m_moveToPrevBookBtnRect.Contain(pos) )
	{
		m_idMouseOver = ID_BTN_MOVE_TO_PREV_PAGE;
	}

	if(prevIdMouseOver != m_idMouseOver)
		TryRender();

	if( m_offsetSeekbarThumbPos.has_value())
	{
		if(evt.LeftIsDown() )
		{
			const auto maxValue = wxGetApp().GetPageCount() - 1;
			if(maxValue == 0)
				return;
			const Rect& seekBarRect = m_seekBarRect;
			const float scale = GetDPIScaleFactor();
			auto& offset = *m_offsetSeekbarThumbPos;
			const int x = pos.x - offset.x;
			const float gap = seekBarRect.GetWidth() / wxGetApp().GetPageCount();
			const int value = round((x - seekBarRect.left ) / gap);
			if ( value != m_valueSeekBar )
			{
				m_valueSeekBar = value;
				if ( m_valueSeekBar < 0 )
				{
					m_valueSeekBar = 0;
				}
				else if(m_valueSeekBar >= wxGetApp().GetPageCount())
				{
					m_valueSeekBar = wxGetApp().GetPageCount() - 1;
				}

				TryRender();
				auto scrollEvent = wxScrollEvent(wxEVT_SCROLL_THUMBTRACK , wxID_ANY , m_valueSeekBar , wxHORIZONTAL);
				scrollEvent.SetEventObject(this);
				scrollEvent.ResumePropagation(wxEVENT_PROPAGATE_MAX);
				m_pView->ProcessEvent(scrollEvent);
			}

			return;
		}

		m_offsetSeekbarThumbPos = std::nullopt;
	}

	if(m_panelRect.Contain(pos))
	{
		if(!m_shownControlPanel)
		{
			m_shownControlPanel = true;
			wxGetApp().GetUpdateSystem().AddWorker(new ShowWithFadeInWorker(this));
		}
	}
	else
	{
		if(m_shownControlPanel)
		{
			m_shownControlPanel = false;
			wxGetApp().GetUpdateSystem().AddWorker(new HideWithFadeOutWorker(this));
		}
	}
}

void ComicZipViewerFrame::OnLMouseDown(wxMouseEvent& evt)
{
	m_offsetSeekbarThumbPos = std::nullopt;
	const wxPoint pos = evt.GetPosition();

	m_latestHittenButtonId = wxID_ANY;
	
	float xxyy[4]
	{
		static_cast<float>(pos.x),
		static_cast<float>(pos.x),
		static_cast<float>(pos.y),
		static_cast<float>(pos.y)
	};

	float rect[4]
	{
		m_originalBtnRect.left,
		m_originalBtnRect.right,
		m_originalBtnRect.top,
		m_originalBtnRect.bottom,
	};

	if( m_originalBtnRect.Contain(pos))
	{
		m_latestHittenButtonId = ID_BTN_ORIGINAL;
	}
	else if( m_fitPageBtnRect.Contain(pos) )
	{
		m_latestHittenButtonId = ID_BTN_FIT_PAGE;
	}
	else if( m_fitWidthBtnRect.Contain(pos) )
	{
		m_latestHittenButtonId = ID_BTN_FIT_WIDTH;
	}
	else if( m_addMarkBtnRect.Contain(pos) )
	{
		m_latestHittenButtonId = ID_BTN_ADD_MARK;
	}
	else if( m_bookmarkViewBtnRect.Contain(pos) )
	{
		m_latestHittenButtonId = ID_BTN_BOOKMARK_VIEW;
	}
	else if(m_moveToNextBookBtnRect.Contain(pos) )
	{
		m_latestHittenButtonId = ID_BTN_MOVE_TO_NEXT_PAGE;
	}
	else if(m_moveToPrevBookBtnRect.Contain(pos))
	{
		m_latestHittenButtonId = ID_BTN_MOVE_TO_PREV_PAGE;
	}


	if( m_latestHittenButtonId != wxID_ANY)
		return;

	const Rect& seekBarRect = m_seekBarRect;
	const auto pageCount = wxGetApp().GetPageCount();
	if ( pageCount == 0 )
		return;

	const auto maxValue = pageCount - 1;
	if(maxValue == 0)
		return;

	const float scale = GetDPIScaleFactor();
	Rect seekBarJumpHitRect = seekBarRect;
	seekBarJumpHitRect.top -= SEEK_BAR_TRACK_HEIGHT * 0.5f * scale;
	seekBarJumpHitRect.bottom += SEEK_BAR_TRACK_HEIGHT * 0.5f * scale;
	const float y = seekBarRect.top + SEEK_BAR_TRACK_HEIGHT * 0.5f * scale;
	const float diffY = pos.y - y;
	if(seekBarJumpHitRect.left <= pos.x && pos.x <= seekBarJumpHitRect.right
		&& seekBarJumpHitRect.top <= pos.y && pos.y <= seekBarJumpHitRect.bottom)
	{
		const float gap = seekBarRect.GetWidth() / maxValue;
		const int value = round(( pos.x - seekBarRect.left ) / gap);
		const float x = seekBarRect.GetWidth() * ((float)value / maxValue) + SEEK_BAR_TRACK_HEIGHT * 0.5f * scale + seekBarRect.left;
		m_offsetSeekbarThumbPos = wxPoint(pos.x - x , diffY);
		if(m_valueSeekBar != value)
		{
			m_valueSeekBar = value;
			TryRender();
			auto scrollEvent = wxScrollEvent(wxEVT_SCROLL_THUMBTRACK , wxID_ANY , value , wxHORIZONTAL);
			scrollEvent.SetEventObject(this);
			m_pView->ProcessEvent(scrollEvent);
		}
	}
	else
	{
		const float percent = (float)m_valueSeekBar / ( float ) maxValue;
		const float x = seekBarRect.GetWidth() * percent + SEEK_BAR_TRACK_HEIGHT * 0.5f * scale + seekBarRect.left;
		const float radius = SEEK_BAR_THUMB_RADIUS * scale;
		const float diffX = pos.x - x;
		if(diffX * diffX + diffY * diffY <= radius * radius)
		{
			m_offsetSeekbarThumbPos = wxPoint(diffX , diffY);
		}
	}

	if(!m_offsetSeekbarThumbPos.has_value())
	{
		m_prevMousePosition = pos;
	}
}

void ComicZipViewerFrame::OnLMouseUp(wxMouseEvent& evt)
{
	m_prevMousePosition = std::nullopt;
	m_offsetSeekbarThumbPos = std::nullopt;
	const auto pos = evt.GetPosition();
	if ( m_latestHittenButtonId == wxID_ANY )
		return;

	wxWindowID hitId = wxID_ANY;
	if ( m_originalBtnRect.Contain(pos) )
	{
		hitId = ID_BTN_ORIGINAL;
	}
	else if ( m_fitPageBtnRect.Contain(pos) )
	{
		hitId = ID_BTN_FIT_PAGE;
	}
	else if ( m_fitWidthBtnRect.Contain(pos) )
	{
		hitId = ID_BTN_FIT_WIDTH;
	}
	else if ( m_addMarkBtnRect.Contain(pos) )
	{
		hitId = ID_BTN_ADD_MARK;
	}
	else if ( m_bookmarkViewBtnRect.Contain(pos) )
	{
		hitId = ID_BTN_BOOKMARK_VIEW;
	}
	else if ( m_moveToNextBookBtnRect.Contain(pos) )
	{
		hitId = ID_BTN_MOVE_TO_NEXT_PAGE;
	}
	else if ( m_moveToPrevBookBtnRect.Contain(pos) )
	{
		hitId = ID_BTN_MOVE_TO_PREV_PAGE;
	}

	if ( m_latestHittenButtonId != hitId )
		return;

	wxCommandEvent clickedEvent(wxEVT_BUTTON , hitId);
	m_latestHittenButtonId = wxID_ANY;
	clickedEvent.SetEventObject(this);
	m_pView->ProcessEvent(clickedEvent);
}

void ComicZipViewerFrame::OnShown(wxShowEvent& evt)
{

}

void ComicZipViewerFrame::OnDpiChanged(wxDPIChangedEvent& event)
{
	GenerateIconBitmaps();
}

void ComicZipViewerFrame::UpdateClientSize(const wxSize& sz)
{
	m_clientSize = sz;
	const float scale = GetDPIScaleFactor();
	static constexpr float PADDING_CONTROL_PANEL = 5.f;
	const float xywh[ 4 ]
	{
		5 * scale,
		sz.y - 5 * scale - 150 * scale,
		sz.x - 10 * scale,
		150 * scale
	};

	m_panelRect.left = PADDING_CONTROL_PANEL * scale;
	m_panelRect.right = sz.x - PADDING_CONTROL_PANEL * 2 * scale;
	m_panelRect.top = sz.y - (PADDING_CONTROL_PANEL + 150.f) * scale;
	m_panelRect.bottom = sz.y - PADDING_CONTROL_PANEL * scale;
	m_seekBarRect.left = m_panelRect.left + SEEK_BAR_PADDING * scale;
	m_seekBarRect.right = m_panelRect.right - SEEK_BAR_PADDING * scale;
	m_seekBarRect.top = m_panelRect.top + SEEK_BAR_PADDING * scale;
	m_seekBarRect.bottom = m_seekBarRect.top + SEEK_BAR_TRACK_HEIGHT * scale;

	m_moveToPrevBookBtnRect.left = m_panelRect.left + 10 * scale;
	m_moveToPrevBookBtnRect.right = m_moveToPrevBookBtnRect.left + 64 * scale;
	m_moveToPrevBookBtnRect.top = m_seekBarRect.bottom + 10 * scale;
	m_moveToPrevBookBtnRect.bottom = m_moveToPrevBookBtnRect.top + 64 * scale;

	m_moveToNextBookBtnRect.left = m_moveToPrevBookBtnRect.right;
	m_moveToNextBookBtnRect.right = m_moveToNextBookBtnRect.left + 64 * scale;
	m_moveToNextBookBtnRect.top = m_seekBarRect.bottom + 10 * scale;
	m_moveToNextBookBtnRect.bottom = m_moveToNextBookBtnRect.top + 64 * scale;

	m_originalBtnRect.left = m_moveToNextBookBtnRect.right;
	m_originalBtnRect.right = m_originalBtnRect.left + 64 * scale;
	m_originalBtnRect.top = m_seekBarRect.bottom + 10 * scale;
	m_originalBtnRect.bottom = m_originalBtnRect.top + 64 * scale;

	m_fitPageBtnRect.left = m_originalBtnRect.right;
	m_fitPageBtnRect.right = m_fitPageBtnRect.left + 64 * scale;
	m_fitPageBtnRect.top = m_seekBarRect.bottom + 10 * scale;
	m_fitPageBtnRect.bottom = m_fitPageBtnRect.top + 64 * scale;

	m_fitWidthBtnRect.left = m_fitPageBtnRect.right;
	m_fitWidthBtnRect.right = m_fitWidthBtnRect.left + 64 * scale;
	m_fitWidthBtnRect.top = m_seekBarRect.bottom + 10 * scale;
	m_fitWidthBtnRect.bottom = m_fitWidthBtnRect.top + 64 * scale;

	m_bookmarkViewBtnRect.right = m_panelRect.right - 10 * scale;
	m_bookmarkViewBtnRect.left = m_bookmarkViewBtnRect.right - 64 * scale;
	m_bookmarkViewBtnRect.top = m_seekBarRect.bottom + 10 * scale;
	m_bookmarkViewBtnRect.bottom = m_bookmarkViewBtnRect.top + 64 * scale;

	m_addMarkBtnRect.right = m_bookmarkViewBtnRect.left;
	m_addMarkBtnRect.left = m_addMarkBtnRect.right - 64 * scale;
	m_addMarkBtnRect.top = m_seekBarRect.bottom + 10 * scale;
	m_addMarkBtnRect.bottom = m_addMarkBtnRect.top + 64 * scale;
	UpdateScaledImageSize();

	m_toastSystem.SetClientSize(GetDPIScaleFactor(), sz);
}

void ComicZipViewerFrame::GenerateIconBitmaps()
{
	wxSize totalSize{};
	std::vector<wxImage> icons;
	for(auto& pair: m_iconBitmapInfo)
	{
		auto& bundle = std::get<0>(pair.second);
		auto image = bundle.GetBitmapFor(this).ConvertToImage();
		icons.push_back(image);
		auto& rect = std::get<1>(pair.second);
		rect.left = 0;
		rect.right = image.GetWidth();
		rect.top = totalSize.y;
		rect.bottom = rect.top + image.GetHeight();

		totalSize.y += image.GetHeight();
		totalSize.x = std::max(totalSize.x, image.GetWidth());
	}

	// Create Atlas
	ComPtr<ID3D11Device> d3dDevice;
	ComPtr<ID3D11DeviceContext> d3dContext;
	ComPtr<ID2D1DeviceContext1> d2dContext;
	GetD3DDevice(&d3dDevice);
	GetD3DDeviceContext(&d3dContext);
	GetD2DDeviceContext(&d2dContext);
	HRESULT hr;
	ComPtr<ID2D1Bitmap1> bitmap;
	ComPtr<ID3D11Texture2D> atlasTexture2d;
	ComPtr<IDXGISurface> atlasSurface;
	D3D11_TEXTURE2D_DESC texture2dDesc{};
	texture2dDesc.ArraySize = 1;
	texture2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texture2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texture2dDesc.MipLevels = 1;
	texture2dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	texture2dDesc.Usage = D3D11_USAGE_DYNAMIC;
	texture2dDesc.SampleDesc.Count = 1;
	texture2dDesc.Width = totalSize.GetWidth();
	texture2dDesc.Height = totalSize.GetHeight();
	hr = d3dDevice->CreateTexture2D(&texture2dDesc, nullptr, &atlasTexture2d);
	hr = atlasTexture2d.As(&atlasSurface);
	D3D11_MAPPED_SUBRESOURCE mappedRect;
	d3dContext->Map(atlasTexture2d.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRect);
	int y = 0;
	for(auto& image: icons)
	{
		const wxByte* const rowData = image.GetData();
		const wxByte* const alpha = image.GetAlpha();
		const bool hasAlpha = image.HasAlpha();
		const int height = image.GetHeight();
		const int width = image.GetWidth();
		for (int row = 0; row < height; ++row)
		{
			for (int cols = 0; cols < width; ++cols)
			{
				const int idx = row * 3 * width + cols * 3;
				uint32_t rgba = 0;
				rgba = rowData[idx] | rowData[idx + 1] << 8 | rowData[idx + 2] << 16 | (hasAlpha ? alpha[row * width + cols] : 0xFF) << 24;
				*reinterpret_cast<uint32_t*>((uint8_t*)mappedRect.pData + y * mappedRect.RowPitch + cols * 4) = rgba;
			}

			y += 1;
		}
	}

	d3dContext->Unmap(atlasTexture2d.Get(), 0);
	hr = d2dContext->CreateBitmapFromDxgiSurface(atlasSurface.Get(), D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);
	if(FAILED(hr))
	{
		wxLogError(wxS("Failed to create D2D Bitmap"));
		return;
	}

	m_iconAtlas = bitmap;
}

void ComicZipViewerFrame::OnContextMenu(wxContextMenuEvent& evt)
{
	PopupMenu(m_pContextMenu , ScreenToClient(evt.GetPosition()));
}

void ComicZipViewerFrame::UpdateScaledImageSize()
{
	float width;
	float height;
	m_center = D2D1::Point2F(0.f , 0.f);
	m_movableCenterRange = D2D1::SizeF(0.f , 0.f);
	switch(m_imageViewMode )
	{
	case ImageViewModeKind::FIT_WIDTH:
		width = m_clientSize.x * 0.5f;
		height = static_cast< float >( m_imageSize.y ) / static_cast< float >( m_imageSize.x );
		height = width * height;
		m_center = D2D1::Point2F(0.f , height - m_clientSize.y * 0.5f);
		if ( m_center.y < 0.f )
			m_center.y = 0.f;

		m_movableCenterRange.height = m_center.y;
		break;
	case ImageViewModeKind::ORIGINAL:
		width = m_imageSize.x * 0.5f;
		height = m_imageSize.y * 0.5f;
		m_center = D2D1::Point2F(0.f , height - m_clientSize.y * 0.5f);
		if ( m_center.y < 0.f )
			m_center.y = 0.f;

		m_movableCenterRange.height = m_center.y;
		if(m_imageSize.x > m_clientSize.x)
		{
			m_movableCenterRange.width = width - m_clientSize.x * 0.5f;
		}
		
		break;

	case ImageViewModeKind::FIT_PAGE:
		if ( m_imageSize.x / ( float ) m_imageSize.y <= m_clientSize.x / ( float ) m_clientSize.y )
		{
			height = m_clientSize.y * 0.5f;
			width = static_cast< float >( m_imageSize.x ) / static_cast< float >( m_imageSize.y );
			width = width * height;
		}
		else
		{
			width = m_clientSize.x * 0.5f;
			height = static_cast< float >( m_imageSize.y ) / static_cast< float >( m_imageSize.x );
			height = width * height;
		}

		break;
	}

	m_centerCorrectionValue.x = m_clientSize.x * 0.08f;
	m_centerCorrectionValue.y = m_clientSize.y * 0.08f;
	m_scaledImageSize = D2D1::SizeF(width , height);
	m_scale = 1.f;
}

void ComicZipViewerFrame::OnMouseWheel(wxMouseEvent& evt)
{
	if(!evt.ControlDown())
	{
		if(evt.GetWheelAxis() == wxMOUSE_WHEEL_VERTICAL)
		{
			ScrollImageVertical( m_clientSize.y * 0.08f * (static_cast<float>(evt.GetWheelRotation()) / 120.f), true);
		}
		else
		{
			ScrollImageHorizontal(m_clientSize.x * -0.08f * (static_cast<float>(evt.GetWheelRotation()) / 120.f), true);
		}
	}
	else
	{
		// TODO: Scale operation
		ChangeScale(evt.GetWheelRotation() / 12.f, evt.GetPosition());
	}
}

void ComicZipViewerFrame::ScrollImageHorizontal(float delta, bool movableOtherPage)
{
	const float prevCenterX = m_center.x;
	const float currentCenterX = prevCenterX + delta;
	m_center.x = currentCenterX;
	float diff = abs(currentCenterX) - m_movableCenterRange.width;
	const bool isOverScroll = diff > 0;
	if(isOverScroll)
	{
		m_center.x = copysign(m_movableCenterRange.width, delta);
		if(movableOtherPage)
		{
			m_centerCorrectionValue.x -= std::abs(delta);
			if(m_centerCorrectionValue.x <= 0.f )
			{
				m_centerCorrectionValue.x = m_clientSize.x * 0.08f;
				m_centerCorrectionValue.y = m_clientSize.y * 0.08f;
				wxCommandEvent event{ wxEVT_BUTTON, wxID_ANY};
				event.SetEventObject(this);
				if(delta > 0.f)
				{
					event.SetId(wxID_BACKWARD);
				}
				else
				{
					event.SetId(wxID_FORWARD);
				}

				m_pView->ProcessEvent(event);
				return;
			}

		}
	}
	else if(movableOtherPage)
	{
		m_centerCorrectionValue.x = m_clientSize.x * 0.08f;
		m_centerCorrectionValue.y = m_clientSize.y * 0.08f;
	}

	TryRender();
}

void ComicZipViewerFrame::ScrollImageVertical(float delta, bool movableOtherPage)
{
	const float prevCenterY = m_center.y;
	const float currentCenterY = prevCenterY + delta;
	m_center.y = currentCenterY;
	float diff = abs(m_center.y) - m_movableCenterRange.height;
	const bool isOverScroll = diff > 0;
	if(isOverScroll)
	{
		m_center.y = copysign(m_movableCenterRange.height, delta);
		if(movableOtherPage)
		{
			m_centerCorrectionValue.y -= std::abs(delta);
			if(m_centerCorrectionValue.y <= 0.f )
			{
				m_centerCorrectionValue.x = m_clientSize.x * 0.08f;
				m_centerCorrectionValue.y = m_clientSize.y * 0.08f;
				wxCommandEvent event{ wxEVT_BUTTON, wxID_ANY};
				event.SetEventObject(this);
				if(delta > 0)
				{
					event.SetId(wxID_BACKWARD);
				}
				else
				{
					event.SetId(wxID_FORWARD);
				}

				m_pView->ProcessEvent(event);
				return;
			}
		}

	}
	else if(movableOtherPage)
	{
		m_centerCorrectionValue.x = m_clientSize.x * 0.5f * 0.49f;
		m_centerCorrectionValue.y = m_clientSize.y * 0.5f * 0.49f;
	}

	TryRender();
}

void ComicZipViewerFrame::OnMenu(wxCommandEvent& evt)
{
	auto id = evt.GetId();
	if(ID_MENU_RECENT_FILE_ITEM_BEGIN <= id && id <= ID_MENU_RECENT_FILE_ITEM_END)
	{
		evt.SetString(m_recentFileList[id - ID_MENU_RECENT_FILE_ITEM_BEGIN ]);
	}

	m_pView->ProcessEventLocally(evt);
}

void ComicZipViewerFrame::ChangeScale(float delta, const wxPoint& center)
{
	delta *= 0.01f;
	float x = m_center.x / m_scaledImageSize.width;
	float y = m_center.y / m_scaledImageSize.height;
	float scale = m_scale;
	D2D_SIZE_F imageSize = m_scaledImageSize;
	imageSize.width *= 1.f / scale;
	imageSize.height *= 1.f / scale;
	scale += delta;
	if(signbit(scale) || scale <= 0.01f)
	{
		return;
	}

	m_scale = scale;
	m_scaledImageSize.width = imageSize.width * scale;
	m_scaledImageSize.height = imageSize.height * scale;
	m_movableCenterRange.width = m_scaledImageSize.width - m_clientSize.x * 0.5f;
	m_movableCenterRange.height = m_scaledImageSize.height - m_clientSize.y * 0.5f;
	if(signbit(m_movableCenterRange.width))
	{
		m_movableCenterRange.width = 0.f;
	}

	if(signbit(m_movableCenterRange.height))
	{
		m_movableCenterRange.height = 0.f;
	}

	x *= m_scaledImageSize.width;
	y *= m_scaledImageSize.height;
	x = std::copysign(std::min(m_movableCenterRange.width, std::abs(x)), x);
	y = std::copysign(std::min(m_movableCenterRange.height, std::abs(y)), y);
	m_center.x = x;
	m_center.y = y;
	TryRender();
}

void ComicZipViewerFrame::SetSeekBarPos(int value)
{
	m_valueSeekBar = value;
	m_offsetSeekbarThumbPos = std::nullopt;
	TryRender();
}

void ComicZipViewerFrame::SetImageViewMode(ImageViewModeKind mode)
{
	m_imageViewMode = mode;
	UpdateScaledImageSize();
	TryRender();
}

void ComicZipViewerFrame::SetPageIsMarked(bool value)
{
	m_pageIsMarked = value;
	TryRender();
}

void ComicZipViewerFrame::SetRecentFiles(std::vector<std::tuple<wxString, wxString>>&& list)
{
	auto items = m_pRecentFileMenu->GetMenuItems();
	for(auto& it: items)
	{
		m_pRecentFileMenu->Delete(it);
	}

	std::unordered_map<std::wstring_view, wxMenuItem*> roots; // <prefix>
	std::unordered_map<wxString, std::tuple<wxString*, wxString*> > prefixTreeTextTable; // <NameWithExt, (Prefix, PageName)>

	wxWindowID id = ID_MENU_RECENT_FILE_ITEM_BEGIN;
	for(auto& tuple: list)
	{
		auto& prefix = std::get<0>(tuple);
		wxFileName prefixFileName(prefix);
		wxString label;
		auto nameWithExt = prefixFileName.GetFullName();
		if(auto it = prefixTreeTextTable.find(nameWithExt); it != prefixTreeTextTable.end())
		{
			label = prefix;
			auto& pPrevPrefix = std::get<0>(it->second);
			if(pPrevPrefix != nullptr)
			{
				auto& menuItem = roots[std::wstring_view(pPrevPrefix->wx_str())];
				menuItem->SetItemLabel(wxString::Format(wxS("%s > %s"), *pPrevPrefix, *std::get<1>(it->second)));
				pPrevPrefix = nullptr;
			}
		}
		else
		{
			label = nameWithExt;
			prefixTreeTextTable.emplace(nameWithExt, std::make_tuple(&prefix, &std::get<1>(tuple)));
		}

		auto item = m_pRecentFileMenu->Append(id, wxString::Format(wxS("%s > %s"), label, std::get<1>(tuple)));
		roots.emplace(std::get<0>(tuple).wx_str(), item);
		id += 1;
	}

	m_recentFileList.clear();
	m_recentFileList.reserve(list.size());
	for(auto& tuple: list)
	{
		m_recentFileList.emplace_back(std::move(std::get<0>(tuple)));
	}
}

BEGIN_EVENT_TABLE(ComicZipViewerFrame , CustomCaptionFrame)
	EVT_SIZE(ComicZipViewerFrame::OnSize)
	EVT_KEY_DOWN(ComicZipViewerFrame::OnKeyDown)
	EVT_KEY_UP(ComicZipViewerFrame::OnKeyUp)
	EVT_LEFT_DOWN(ComicZipViewerFrame::OnLMouseDown)
	EVT_LEFT_UP(ComicZipViewerFrame::OnLMouseUp)
	EVT_MOTION(ComicZipViewerFrame::OnMouseMove)
	EVT_LEAVE_WINDOW(ComicZipViewerFrame::OnMouseLeave)
	EVT_DPI_CHANGED(ComicZipViewerFrame::OnDpiChanged)
	EVT_CONTEXT_MENU(ComicZipViewerFrame::OnContextMenu)
	EVT_MOUSEWHEEL(ComicZipViewerFrame::OnMouseWheel)
	EVT_MENU(wxID_ANY, ComicZipViewerFrame::OnMenu)
END_EVENT_TABLE()
