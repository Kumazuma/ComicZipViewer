#include "framework.h"
#include "ComicZipViewerFrame.h"
#include "ComicZipViewer.h"
#include "CsRgb24ToRgba32"
#include "CsRgb24WithAlphaToRgba32"
#include <wx/bitmap.h>

wxDEFINE_EVENT(wxEVT_SHOW_CONTROL_PANEL, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_HIDE_CONTROL_PANEL, wxCommandEvent);

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "sqlite.lib")

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

ComicZipViewerFrame::ComicZipViewerFrame()

	: m_isSizing(false)
	, m_enterIsDown(false)
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
	auto ret = wxFrame::Create(nullptr, wxID_ANY, wxS("ComicZipViewer"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS);
	if ( !ret )
	{
		return false;
	}

	UpdateClientSize(GetClientSize());
	m_pContextMenu = new wxMenu();
	m_pContextMenu->Append(wxID_OPEN, wxS("Open(&O)"));


	auto* pImageSizingModeMenu = new wxMenu();
	pImageSizingModeMenu->Append(ID_BTN_ORIGINAL , wxS("original size"));
	pImageSizingModeMenu->Append(ID_BTN_FIT_WIDTH, wxS("Fit window's width"));
	pImageSizingModeMenu->Append(ID_BTN_FIT_PAGE , wxS("Fit window's area"));
	m_pContextMenu->AppendSubMenu(pImageSizingModeMenu , wxS("View modes"));

	m_pContextMenu->Append(wxID_CLOSE , wxS("Close(&C)"));

	wxBitmapBundle a;
	HRESULT hRet;
	HWND hWnd = GetHWND();
	ComPtr<IDXGIFactory2> dxgiFactory = nullptr;
	UINT flags = 0;
	UINT d3d11Flag = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	#if !defined(NDEBUG)
	flags = DXGI_CREATE_FACTORY_DEBUG;
	d3d11Flag = d3d11Flag | D3D11_CREATE_DEVICE_DEBUG;
	#endif
	CreateDXGIFactory2(flags, __uuidof(IDXGIFactory2),  &dxgiFactory);

	D3D_FEATURE_LEVEL featureLevel;
	hRet = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, d3d11Flag, nullptr, 0, D3D11_SDK_VERSION, &m_d3dDevice, &featureLevel, &m_d3dContext);
	if(FAILED(hRet))
	{
		wxLogError(wxS("Failed to create d3d device"));
		return false;
	}
	
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc1{};
	swapChainDesc1.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc1.BufferCount = 3;
	swapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
	swapChainDesc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc1.Height = m_clientSize.GetHeight();
	swapChainDesc1.Width = m_clientSize.GetWidth();
	swapChainDesc1.SampleDesc.Count = 1;
	swapChainDesc1.SampleDesc.Quality = 0;
	swapChainDesc1.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc1.Stereo = false;
	swapChainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreenDesc{};
	fullScreenDesc.RefreshRate.Numerator = 0;
	fullScreenDesc.RefreshRate.Denominator = 1;
	fullScreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	fullScreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	fullScreenDesc.Windowed = true;
	dxgiFactory->CreateSwapChainForHwnd(m_d3dDevice.Get(), hWnd, &swapChainDesc1, &fullScreenDesc, nullptr, &m_swapChain);
	m_swapChain->GetParent(__uuidof(IDXGIFactory2),  &dxgiFactory);
	dxgiFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);

	ComPtr<IDXGISurface> surface;
	ComPtr<IDXGIDevice> dxgiDevice;
	m_d3dDevice.As(&dxgiDevice);
	m_swapChain->GetBuffer(0, __uuidof(IDXGISurface), &surface);
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(m_d2dFactory), &m_d2dFactory);
	hRet = m_d2dFactory->CreateDevice(dxgiDevice.Get(), &m_d2dDevice);
   
	hRet = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext);
	 m_d2dContext->SetDpi(96.f, 96.f);
	hRet = m_d2dContext->CreateBitmapFromDxgiSurface(
		surface.Get(),
		D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
		&m_targetBitmap);

	hRet = m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.f, 0.f, 0.f, 0.75f), &m_d2dBlackBrush);
	hRet = m_d2dContext->CreateSolidColorBrush(D2D1::ColorF( 0.f, 0.47f , 0.83f) , &m_d2dBlueBrush);
	hRet = m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(1.f , 1.f , 1.f) , &m_d2dWhiteBrush);
	hRet = m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.7f , 0.7f , 0.7f, 0.75f) , &m_d2dBackGroundWhiteBrush);
	hRet = m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(0.2f , 0.2f , 0.2f) , &m_d2dGrayBrush);
	m_d2dFactory->CreateStrokeStyle(D2D1::StrokeStyleProperties(), nullptr, 0, &m_d2dSimpleStrokeStyle);
	hRet = m_d2dContext->CreateLayer(&m_controlPanelLayer);
	m_d2dContext->SetTarget(m_targetBitmap.Get());

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

	m_d3dDevice->CreateComputeShader(CsRgb24ToRgba32, sizeof(CsRgb24ToRgba32), nullptr, &m_d3dCsRgb24ToRgba32);
	m_d3dDevice->CreateComputeShader(CsRgb24WithAlphaToRgba32 , sizeof(CsRgb24WithAlphaToRgba32) , nullptr , &m_d3dCsRgb24WithAlphaToRgba32);
	return true;
}

WXLRESULT ComicZipViewerFrame::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
{
	switch(message)
	{
	case WM_ENTERSIZEMOVE:
		m_isSizing = true;
		break;
	case WM_EXITSIZEMOVE:
		m_isSizing = false;
		UpdateClientSize(GetClientSize());
		ResizeSwapChain(m_clientSize);
		Render();
		break;

	case WM_SIZE:
		{
			//if(IsFullScreen())
			//{
			//    /// ResizeSwapChain(GetClientSize());
			//    return DefWindowProcW(GetHWND(), message, wParam, lParam);
			//}
		}
	}

	return wxFrame::MSWWindowProc(message, wParam, lParam);
}

void ComicZipViewerFrame::ShowImage(const wxImage& image)
{
	const auto hasAlpha = image.HasAlpha();
	const auto alphaMode = hasAlpha ? D2D1_ALPHA_MODE_PREMULTIPLIED : D2D1_ALPHA_MODE_IGNORE;
	const auto size = image.GetSize();
	bool needCreationBitmap = m_bitmap == nullptr;
	if(!needCreationBitmap)
	{
		const auto bitmapSize = m_bitmap->GetPixelSize();
		needCreationBitmap = size.x != bitmapSize.width || size.y != bitmapSize.height;
		needCreationBitmap = needCreationBitmap || m_bitmap->GetPixelFormat().alphaMode != alphaMode;

		if(needCreationBitmap)
		{
			m_bitmap.Reset();
		}
	}

	HRESULT hr;
	ComPtr<ID2D1Bitmap1> bitmap;
	ComPtr<ID3D11Texture2D> texture2d;
	D3D11_TEXTURE2D_DESC texture2dDesc{};

	if(needCreationBitmap)
	{
		texture2dDesc.ArraySize = 1;
		texture2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		texture2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texture2dDesc.MipLevels = 1;
		texture2dDesc.CPUAccessFlags = 0;
		texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
		texture2dDesc.SampleDesc.Count = 1;
		texture2dDesc.Width = size.GetWidth();
		texture2dDesc.Height = size.GetHeight();
		hr = m_d3dDevice->CreateTexture2D(&texture2dDesc, nullptr, &texture2d);

		hr = m_d3dDevice->CreateUnorderedAccessView(texture2d.Get() , nullptr , &m_d3dUavTexture2d);
		assert(SUCCEEDED(hr));
	}
	else
	{
		bitmap = m_bitmap;
	}

	ComPtr<ID3D11Texture2D> rgb24Texture;
	ComPtr<ID3D11Texture2D> alphaTexture;
	ComPtr<ID3D11ShaderResourceView> srvRgb24Texture;
	ComPtr<ID3D11ShaderResourceView> srvAlphaTexture;

	texture2dDesc.ArraySize = 1;
	texture2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texture2dDesc.Format = DXGI_FORMAT_R8_UNORM;
	texture2dDesc.MipLevels = 1;
	texture2dDesc.CPUAccessFlags = 0;
	texture2dDesc.Usage = D3D11_USAGE_DEFAULT;
	texture2dDesc.SampleDesc.Count = 1;
	texture2dDesc.Width = size.GetWidth() * 3;
	texture2dDesc.Height = size.GetHeight();
	D3D11_SUBRESOURCE_DATA subResData{};
	const wxByte* const rowData = image.GetData();
	subResData.pSysMem = rowData;
	subResData.SysMemPitch = size.GetWidth() * 3;
	hr = m_d3dDevice->CreateTexture2D(&texture2dDesc , &subResData , &rgb24Texture);
	assert(SUCCEEDED(hr));

	hr = m_d3dDevice->CreateShaderResourceView(rgb24Texture.Get(), nullptr, &srvRgb24Texture);
	assert(SUCCEEDED(hr));

	if(hasAlpha)
	{
		texture2dDesc.Width = size.GetWidth();
		texture2dDesc.Height = size.GetHeight();
		subResData.pSysMem = image.GetAlpha();
		subResData.SysMemPitch = size.GetWidth();

		hr = m_d3dDevice->CreateTexture2D(&texture2dDesc , &subResData , &alphaTexture);
		assert(SUCCEEDED(hr));

		hr = m_d3dDevice->CreateShaderResourceView(alphaTexture.Get() , nullptr , &srvAlphaTexture);
		assert(SUCCEEDED(hr));
	}

	ID3D11ShaderResourceView* srvs[] {srvRgb24Texture.Get(), srvAlphaTexture.Get()};
	ID3D11UnorderedAccessView* uavs[] { m_d3dUavTexture2d.Get()};
	m_d3dContext->CSSetShaderResources(0, 2, srvs);
	m_d3dContext->CSSetUnorderedAccessViews(0, 1, uavs, nullptr);
	if(!hasAlpha)
	{
		m_d3dContext->CSSetShader(m_d3dCsRgb24ToRgba32.Get() , nullptr , 0);
	}
	else
	{
		m_d3dContext->CSSetShader(m_d3dCsRgb24WithAlphaToRgba32.Get() , nullptr , 0);
	}
	
	m_d3dContext->Dispatch(size.GetWidth(), size.GetHeight(), 1);
	if(needCreationBitmap)
	{
		ComPtr<IDXGISurface> surface;
		hr = texture2d.As(&surface);
		hr = m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, alphaMode)), &bitmap);
		if(FAILED(hr))
		{
			wxLogError(wxS("Failed to create D2D Bitmap"));
			return;
		}
	}

	m_bitmap = bitmap;
	m_imageSize = size;
	UpdateScaledImageSize();
	TryRender();
}

void ComicZipViewerFrame::OnSize(wxSizeEvent& evt)
{
	if(m_isSizing)
		return;

	UpdateClientSize(GetClientSize());
	ResizeSwapChain(m_clientSize);
	Render();
}

void ComicZipViewerFrame::OnKeyDown(wxKeyEvent& evt)
{
	wxCommandEvent event(wxEVT_BUTTON, wxID_ANY);
	event.SetEventObject(this);
	switch(evt.GetKeyCode())
	{
	case WXK_UP:
		ScrollImageVertical(120);
		return;
	case WXK_DOWN:
		ScrollImageVertical(-120);
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

void ComicZipViewerFrame::ResizeSwapChain(const wxSize clientRect)
{
	
	HRESULT hRet;
	ComPtr<IDXGISurface> surface;
	m_d2dContext->SetTarget(nullptr);
	m_targetBitmap.Reset();
	m_swapChain->ResizeBuffers(0, clientRect.GetWidth(), clientRect.GetHeight(), DXGI_FORMAT_UNKNOWN, 0);
	m_swapChain->GetBuffer(0, __uuidof(IDXGISurface), &surface);
	hRet = m_d2dContext->CreateBitmapFromDxgiSurface(
		surface.Get(),
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
			D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
		&m_targetBitmap);

	m_d2dContext->SetTarget(m_targetBitmap.Get());
}

void ComicZipViewerFrame::Render()
{
	if(IsFrozen())
		return;

	m_willRender = false;
	HRESULT hRet;
	m_d2dContext->BeginDraw();
	m_d2dContext->Clear(D2D1::ColorF(0.3f, 0.3f, 0.3f, 1.f));
	if(m_bitmap)
	{
		m_d2dContext->SetTransform(D2D1::Matrix3x2F::Translation(m_clientSize.x * 0.5f , m_clientSize.y * 0.5f) * D2D1::Matrix3x2F::Translation(m_center.x, m_center.y));
		m_d2dContext->DrawBitmap(m_bitmap.Get() , D2D1::RectF(-m_scaledImageSize.width , -m_scaledImageSize.height , m_scaledImageSize.width , m_scaledImageSize.height), 1.f, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
		m_d2dContext->SetTransform(D2D1::Matrix3x2F::Identity());
	}

	if ( m_alphaControlPanel > 0.f )
	{
		const float scale = GetDPIScaleFactor();
		const auto& size = m_clientSize;
		const Rect& seekBarRect = m_seekBarRect;
		const float seekBarX = m_panelRect.left + SEEK_BAR_PADDING * scale;
		const float seekBarWidth = m_panelRect.GetWidth() - SEEK_BAR_PADDING * 2 * scale;
		m_d2dContext->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect() , nullptr , D2D1_ANTIALIAS_MODE_PER_PRIMITIVE , D2D1::IdentityMatrix() , m_alphaControlPanel) , m_controlPanelLayer.Get());
		m_d2dContext->FillRectangle(m_panelRect , m_d2dBackGroundWhiteBrush.Get());
		m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(seekBarRect , SEEK_BAR_TRACK_HEIGHT * 0.5f * scale , SEEK_BAR_TRACK_HEIGHT * 0.5f * scale) , m_d2dGrayBrush.Get());
		if ( wxGetApp().GetPageCount() >= 1 )
		{
			const float percent = m_valueSeekBar / ( float ) ( wxGetApp().GetPageCount() - 1 );
			const float bar = seekBarRect.GetWidth() * percent + SEEK_BAR_TRACK_HEIGHT * 0.5f * scale + seekBarRect.left;
			const float thumbCenterY = seekBarRect.top + SEEK_BAR_TRACK_HEIGHT * 0.5f * scale;
			Rect valueRect = seekBarRect;
			valueRect.right = bar;
			m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(valueRect , SEEK_BAR_TRACK_HEIGHT * 0.5f * scale , SEEK_BAR_TRACK_HEIGHT * 0.5f * scale) , m_d2dBlueBrush.Get());
			m_d2dContext->FillEllipse(D2D1::Ellipse(D2D1::Point2F(bar , thumbCenterY) , SEEK_BAR_THUMB_RADIUS * scale , SEEK_BAR_THUMB_RADIUS * scale) , m_d2dWhiteBrush.Get());
			m_d2dContext->FillEllipse(D2D1::Ellipse(D2D1::Point2F(bar , thumbCenterY) , 5.0f * scale , 5.0f * scale) , m_d2dBlueBrush.Get());
			m_d2dContext->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(bar , thumbCenterY) , SEEK_BAR_THUMB_RADIUS * scale , SEEK_BAR_THUMB_RADIUS * scale) , m_d2dBlueBrush.Get() , 0.5f , m_d2dSimpleStrokeStyle.Get());
		}

		if ( m_imageViewMode == ImageViewModeKind::ORIGINAL )
			m_d2dContext->FillRectangle(m_originalBtnRect , m_d2dWhiteBrush.Get());

		m_d2dContext->DrawBitmap(m_iconAtlas.Get() , m_originalBtnRect , 1.f , D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ ID_SVG_PAGE ]));

		if ( m_imageViewMode == ImageViewModeKind::FIT_PAGE )
			m_d2dContext->FillRectangle(m_fitPageBtnRect , m_d2dWhiteBrush.Get());

		m_d2dContext->DrawBitmap(m_iconAtlas.Get() , m_fitPageBtnRect , 1.f , D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ ID_SVG_FIT_PAGE ]));

		if ( m_imageViewMode == ImageViewModeKind::FIT_WIDTH )
			m_d2dContext->FillRectangle(m_fitWidthBtnRect , m_d2dWhiteBrush.Get());

		m_d2dContext->DrawBitmap(m_iconAtlas.Get() , m_fitWidthBtnRect , 1.f , D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ ID_SVG_FIT_WIDTH ]));

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

			m_d2dContext->FillRectangle(*rc , m_d2dWhiteBrush.Get());
		}

		m_d2dContext->DrawBitmap(m_iconAtlas.Get(), m_bookmarkViewBtnRect, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR, std::get<1>(m_iconBitmapInfo[ID_SVG_TEXT_BULLET_LIST_SQUARE]));
		m_d2dContext->DrawBitmap(m_iconAtlas.Get(), m_moveToPrevBookBtnRect, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ ID_SVG_MOVE_TO_PREV_BOOK ]));
		m_d2dContext->DrawBitmap(m_iconAtlas.Get() , m_moveToNextBookBtnRect , 1.f , D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ ID_SVG_MOVE_TO_NEXT_BOOK ]));

		if(m_pageIsMarked)
		{
			m_d2dContext->DrawBitmap(m_iconAtlas.Get(), m_addMarkBtnRect, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ID_SVG_MARKED_PAGE]));
		}
		else
		{
			m_d2dContext->DrawBitmap(m_iconAtlas.Get(), m_addMarkBtnRect, 1.f, D2D1_BITMAP_INTERPOLATION_MODE_NEAREST_NEIGHBOR , std::get<1>(m_iconBitmapInfo[ID_SVG_UNMARKED_PAGE]));
		}


		m_d2dContext->PopLayer();
	}

	hRet = m_d2dContext->EndDraw();
	hRet = m_swapChain->Present(1, 0);
}

void ComicZipViewerFrame::Fullscreen()
{
	ShowFullScreen(true);
	ComPtr<IDXGIOutput> output;
	m_swapChain->GetContainingOutput(&output);
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

inline int64_t GetTickFrequency()
{
	LARGE_INTEGER largeInteger{};
	QueryPerformanceFrequency(&largeInteger);
	return largeInteger.QuadPart;
}

inline int64_t GetTick()
{
	LARGE_INTEGER largeInteger{};
	QueryPerformanceCounter(&largeInteger);
	return largeInteger.QuadPart;
}

void ComicZipViewerFrame::OnShowControlPanel(wxCommandEvent& event)
{
	const int64_t frequency = GetTickFrequency();
	int64_t latestTick = GetTick();
	while(m_shownControlPanel && !IsBeingDeleted())
	{
		if(m_alphaControlPanel >= 1.f)
		{
			m_alphaControlPanel = 1.f;
			Render();
			break;
		}

		int64_t current = GetTick();
		const int64_t diff = current - latestTick;
		latestTick = current;
		float delta = ((diff * 4096ll) / frequency) * ( 1.f / 4096.f);
		m_alphaControlPanel += delta * 20.f;
		Render();
		wxYieldIfNeeded();
	}
}

void ComicZipViewerFrame::OnHideControlPanel(wxCommandEvent& event)
{
	const int64_t frequency = GetTickFrequency();
	int64_t latestTick = GetTick();
	while(!m_shownControlPanel && !IsBeingDeleted())
	{
		if (m_alphaControlPanel <= 0.f)
		{
			m_alphaControlPanel = 0.f;
			Render();
			break;
		}

		int64_t current = GetTick();
		const int64_t diff = current - latestTick;
		latestTick = current;
		float delta = ((diff * 4096ll) / frequency) * ( 1.f / 4096.f);
		m_alphaControlPanel -= delta * 20.f;
		Render();
		wxYieldIfNeeded();
	}
}

void ComicZipViewerFrame::OnMouseLeave(wxMouseEvent& evt)
{
	if(m_shownControlPanel)
	{
		m_shownControlPanel = false;
		auto evt = new wxCommandEvent(wxEVT_HIDE_CONTROL_PANEL);
		evt->SetEventObject(this);
		QueueEvent(evt);
	}
}

void ComicZipViewerFrame::OnMouseMove(wxMouseEvent& evt)
{
	auto pos = evt.GetPosition();
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
			auto evt = new wxCommandEvent(wxEVT_SHOW_CONTROL_PANEL);
			evt->SetEventObject(this);
			QueueEvent(evt);
		}
	}
	else
	{
		if(m_shownControlPanel)
		{
			m_shownControlPanel = false;
			auto evt = new wxCommandEvent(wxEVT_HIDE_CONTROL_PANEL);
			evt->SetEventObject(this);
			QueueEvent(evt);
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

	const auto pageCount = wxGetApp().GetPageCount();
	if ( pageCount == 0 )
		return;

	const auto maxValue = pageCount - 1;
	if(maxValue == 0)
		return;

	const float scale = GetDPIScaleFactor();
	const Rect& seekBarRect = m_seekBarRect;
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
}

void ComicZipViewerFrame::OnLMouseUp(wxMouseEvent& evt)
{
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
}

void ComicZipViewerFrame::TryRender()
{
	if(IsFrozen())
		return;

	if ( m_willRender )
		return;

	m_willRender = true;
	CallAfter([this]()
	{
		if ( !m_willRender )
			return;

		Render();
	});
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
	hr = m_d3dDevice->CreateTexture2D(&texture2dDesc, nullptr, &atlasTexture2d);
	hr = atlasTexture2d.As(&atlasSurface);
	D3D11_MAPPED_SUBRESOURCE mappedRect;
	m_d3dContext->Map(atlasTexture2d.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRect);
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

	m_d3dContext->Unmap(atlasTexture2d.Get(), 0);
	hr = m_d2dContext->CreateBitmapFromDxgiSurface(atlasSurface.Get(), D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);
	if(FAILED(hr))
	{
		wxLogError(wxS("Failed to create D2D Bitmap"));
		return;
	}

	m_iconAtlas = bitmap;
}

void ComicZipViewerFrame::OnContextMenu(wxContextMenuEvent& evt)
{
	m_pContextMenu->SetEventHandler(m_pView);
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
		width = m_imageSize.x;
		height = m_imageSize.y;
		m_center = D2D1::Point2F(0.f , height - m_clientSize.y * 0.5f);
		if ( m_center.y < 0.f )
			m_center.y = 0.f;

		m_movableCenterRange.height = m_center.y;
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

	m_scaledImageSize = D2D1::SizeF(width , height);
}

void ComicZipViewerFrame::OnMouseWheel(wxMouseEvent& evt)
{
	if(evt.GetWheelAxis() == wxMOUSE_WHEEL_VERTICAL)
	{
		ScrollImageVertical(evt.GetWheelRotation());
	}
	else
	{
		auto r = evt.GetWheelRotation();
		m_center.x += ( r / 120.f ) * m_clientSize.x * 0.33f;
		if ( m_movableCenterRange.width < abs(m_center.x) )
		{
			m_center.x = m_movableCenterRange.width * m_center.x / abs(m_center.x);
		}
		
		TryRender();
	}
}

void ComicZipViewerFrame::ScrollImageVertical(int delta)
{
	const float prevCenterY = m_center.y;
	m_center.y += ( delta / 120.f ) * m_clientSize.y * 0.15f;
	float diff = abs(m_center.y) - m_movableCenterRange.height;
	const bool isOverScroll = diff > 0;
	if(isOverScroll && diff < (m_clientSize.y - 1) * 0.15f)
	{
		m_center.y = m_movableCenterRange.height * m_center.y / abs(m_center.y);
	}
	else if(isOverScroll)
	{
		m_center.y = m_movableCenterRange.height * m_center.y / abs(m_center.y);
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

void ComicZipViewerFrame::DoThaw()
{
	wxFrame::DoThaw();
	Render();
}

BEGIN_EVENT_TABLE(ComicZipViewerFrame , wxFrame)
	EVT_SIZE(ComicZipViewerFrame::OnSize)
	EVT_KEY_DOWN(ComicZipViewerFrame::OnKeyDown)
	EVT_KEY_UP(ComicZipViewerFrame::OnKeyUp)
	EVT_LEFT_DOWN(ComicZipViewerFrame::OnLMouseDown)
	EVT_LEFT_UP(ComicZipViewerFrame::OnLMouseUp)
	EVT_MOTION(ComicZipViewerFrame::OnMouseMove)
	EVT_LEAVE_WINDOW(ComicZipViewerFrame::OnMouseLeave)
	EVT_COMMAND(wxID_ANY, wxEVT_SHOW_CONTROL_PANEL, ComicZipViewerFrame::OnShowControlPanel)
	EVT_COMMAND(wxID_ANY, wxEVT_HIDE_CONTROL_PANEL, ComicZipViewerFrame::OnHideControlPanel)
	EVT_DPI_CHANGED(ComicZipViewerFrame::OnDpiChanged)
	EVT_CONTEXT_MENU(ComicZipViewerFrame::OnContextMenu)
	EVT_MOUSEWHEEL(ComicZipViewerFrame::OnMouseWheel)
END_EVENT_TABLE()
