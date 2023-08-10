#include "framework.h"
#include "ComicZipViewerFrame.h"
#include "ComicZipViewer.h"
#include <wx/bitmap.h>

wxDEFINE_EVENT(wxEVT_SHOW_CONTROL_PANEL, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_HIDE_CONTROL_PANEL, wxCommandEvent);

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxgi.lib")

constexpr float SEEK_BAR_PADDING = 15.f;
constexpr float SEEK_BAR_TRACK_HEIGHT = 5.f;
constexpr float SEEK_BAR_THUMB_RADIUS = 10.f;
ComicZipViewerFrame::ComicZipViewerFrame()
	: m_isSizing(false)
	, m_enterIsDown(false)
	, m_pContextMenu(nullptr)
	, m_alphaControlPanel(0.f)
	, m_shownControlPanel(false)
	, m_offsetSeekbarThumbPos()
	, m_valueSeekBar()
	, m_willRender(false)
	, m_imageSizeMode()
{
}

ComicZipViewerFrame::~ComicZipViewerFrame()
{
	DeletePendingEvents();
	ClearEventHashTable();
	delete m_pContextMenu;
}

bool ComicZipViewerFrame::Create()
{
	auto ret = wxFrame::Create(nullptr, wxID_ANY, wxS("ComicZipViewer"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxWANTS_CHARS);
	UpdateClientSize(GetClientSize());
	m_pContextMenu = new wxMenu();
	m_pContextMenu->Append(wxID_OPEN, wxS("Open(&O)"));
	m_pContextMenu->Append(wxID_CLOSE, wxS("Close(&C)"));
	if (!ret)
	{
		return false;
	}

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
	swapChainDesc1.BufferCount = 2;
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

	m_iconBitmapTable.emplace(wxS("ID_SVG_FIT_PAGE"), std::tuple<wxBitmapBundle, ComPtr<ID2D1Bitmap1>>{});
	m_iconBitmapTable.emplace(wxS("ID_SVG_FIT_WIDTH"), std::tuple<wxBitmapBundle, ComPtr<ID2D1Bitmap1>>{});
	m_iconBitmapTable.emplace(wxS("ID_SVG_PAGE"), std::tuple<wxBitmapBundle, ComPtr<ID2D1Bitmap1>>{});
	for(auto& pair: m_iconBitmapTable)
	{
		auto& bundle = std::get<0>(pair.second);
		bundle = wxBitmapBundle::FromSVGResource(pair.first.data(), wxSize(64, 64));
	}

	GenerateIconBitmaps();
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
	auto size = image.GetSize();
	bool needCreationBitmap = m_bitmap == nullptr;
	if(!needCreationBitmap)
	{
		auto bitmapSize = m_bitmap->GetPixelSize();
		needCreationBitmap = size.x != bitmapSize.width || size.y != bitmapSize.height;
		if(needCreationBitmap)
		{
			m_bitmap.Reset();
		}
	}

	HRESULT hr;
	ComPtr<ID2D1Bitmap1> bitmap;
	ComPtr<ID3D11Texture2D> texture2d;
	ComPtr<IDXGISurface> surface;
	if(needCreationBitmap)
	{
		D3D11_TEXTURE2D_DESC texture2dDesc{};
		texture2dDesc.ArraySize = 1;
		texture2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texture2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texture2dDesc.MipLevels = 1;
		texture2dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		texture2dDesc.Usage = D3D11_USAGE_DYNAMIC;
		texture2dDesc.SampleDesc.Count = 1;
		texture2dDesc.Width = size.GetWidth();
		texture2dDesc.Height = size.GetHeight();
		hr = m_d3dDevice->CreateTexture2D(&texture2dDesc, nullptr, &texture2d);
		hr = texture2d.As(&surface);
		hr = m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);
		if(FAILED(hr))
		{
			wxLogError(wxS("Failed to create D2D Bitmap"));
			return;
		}
	}
	else
	{
		bitmap = m_bitmap;
		bitmap->GetSurface(&surface);
		hr = surface.As(&texture2d);
		assert(texture2d != nullptr);
	}

	D3D11_MAPPED_SUBRESOURCE mappedRect;
	m_d3dContext->Map(texture2d.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRect);
	const wxByte* const rowData = image.GetData();
	const wxByte* const alpha = image.GetAlpha();
	const bool hasAlpha = image.HasAlpha();
	for (int row = 0; row < size.GetHeight(); ++row)
	{
		for (int cols = 0; cols < size.GetWidth(); ++cols)
		{
			const int idx = row * 3 * size.GetWidth() + cols * 3;
			uint32_t rgba = 0;
			rgba = rowData[idx] | rowData[idx + 1] << 8 | rowData[idx + 2] << 16 | (hasAlpha ? alpha[row * size.GetWidth() + cols] : 0xFF) << 24;
			*reinterpret_cast<uint32_t*>((uint8_t*)mappedRect.pData + row * mappedRect.RowPitch + cols * 4) = rgba;
		}
	}

	m_d3dContext->Unmap(texture2d.Get(), 0);
	m_bitmap = bitmap;
	m_imageSize = size;
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

void ComicZipViewerFrame::OnRButtonDown(wxMouseEvent& evt)
{
	m_posPrevRDown = evt.GetPosition();
}

void ComicZipViewerFrame::OnRButtonUp(wxMouseEvent& evt)
{
	auto posPrev = m_posPrevRDown;
	m_posPrevRDown = std::nullopt;
	if(!posPrev.has_value())
		return;

	auto diff = evt.GetPosition() - *posPrev;
	auto l = diff.x * diff.x + diff.y * diff.y;
	if(l > 16)
		return;

	PopupMenu(m_pContextMenu, evt.GetPosition());
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
		D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW, D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
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
		m_d2dContext->DrawBitmap(m_bitmap.Get(), D2D1::RectF(0.f, 0.f, m_imageSize.GetWidth(), m_imageSize.GetHeight()));
	}

	if(m_alphaControlPanel > 0.f)
	{
		const float scale = GetDPIScaleFactor();
		const auto& size = m_clientSize;
		const Rect& seekBarRect = m_seekBarRect;
		const float seekBarX = m_panelRect.left + SEEK_BAR_PADDING * scale;
		const float seekBarWidth = m_panelRect.GetWidth() -  SEEK_BAR_PADDING * 2 * scale;
		m_d2dContext->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), m_alphaControlPanel), m_controlPanelLayer.Get());
		m_d2dContext->FillRectangle(m_panelRect, m_d2dBackGroundWhiteBrush.Get());
		m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(seekBarRect, SEEK_BAR_TRACK_HEIGHT * 0.5f * scale , SEEK_BAR_TRACK_HEIGHT * 0.5f * scale) , m_d2dGrayBrush.Get());
		if( wxGetApp().GetPageCount() >= 1 )
		{
			const float percent = m_valueSeekBar / ( float ) (wxGetApp().GetPageCount() - 1);
			const float bar = seekBarRect.GetWidth() * percent + SEEK_BAR_TRACK_HEIGHT * 0.5f * scale + seekBarRect.left;
			const float thumbCenterY = seekBarRect.top + SEEK_BAR_TRACK_HEIGHT * 0.5f * scale;
			Rect valueRect = seekBarRect;
			valueRect.right = bar;
			m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(valueRect, SEEK_BAR_TRACK_HEIGHT * 0.5f * scale , SEEK_BAR_TRACK_HEIGHT * 0.5f * scale) , m_d2dBlueBrush.Get());
			m_d2dContext->FillEllipse(D2D1::Ellipse(D2D1::Point2F(bar, thumbCenterY), SEEK_BAR_THUMB_RADIUS * scale , SEEK_BAR_THUMB_RADIUS * scale) , m_d2dWhiteBrush.Get());
			m_d2dContext->FillEllipse(D2D1::Ellipse(D2D1::Point2F(bar, thumbCenterY), 5.0f * scale , 5.0f * scale) , m_d2dBlueBrush.Get());
			m_d2dContext->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(bar, thumbCenterY), SEEK_BAR_THUMB_RADIUS * scale , SEEK_BAR_THUMB_RADIUS * scale) , m_d2dBlueBrush.Get(), 0.5f, m_d2dSimpleStrokeStyle.Get());
		}

		Rect iconRect;
		iconRect.left = m_panelRect.left + 10 * scale;
		iconRect.right = iconRect.left + 64 * scale;
		iconRect.top = seekBarRect.bottom + 10 * scale;
		iconRect.bottom = iconRect.top + 64 * scale;
		if(m_imageSizeMode == ImageSizeMode::ORIGINAL)
			m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(iconRect, 5.f * scale, 5.f * scale), m_d2dWhiteBrush.Get());

		m_d2dContext->DrawBitmap(std::get<1>(m_iconBitmapTable[wxS("ID_SVG_PAGE")]).Get(), iconRect);

		iconRect.left = iconRect.right + 10 * scale;
		iconRect.right = iconRect.left + 64 * scale;
		if(m_imageSizeMode == ImageSizeMode::FIT_PAGE)
			m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(iconRect, 5.f * scale, 5.f * scale), m_d2dWhiteBrush.Get());

		m_d2dContext->DrawBitmap(std::get<1>(m_iconBitmapTable[wxS("ID_SVG_FIT_PAGE")]).Get(), iconRect);

		iconRect.left = iconRect.right + 10 * scale;
		iconRect.right = iconRect.left + 64 * scale;
		if(m_imageSizeMode == ImageSizeMode::FIT_WIDTH)
			m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(iconRect, 5.f * scale, 5.f * scale), m_d2dWhiteBrush.Get());

		m_d2dContext->DrawBitmap(std::get<1>(m_iconBitmapTable[wxS("ID_SVG_FIT_WIDTH")]).Get(), iconRect);

		m_d2dContext->PopLayer();
	}

	hRet = m_d2dContext->EndDraw();
	hRet = m_swapChain->Present(0, 0);
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
		m_alphaControlPanel += delta * 40.f;
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
		m_alphaControlPanel -= delta * 40.f;
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
				ProcessWindowEvent(scrollEvent);
			}

			return;
		}

		m_offsetSeekbarThumbPos = std::nullopt;
	}

	if(m_panelRect.left <= pos.x && pos.x <= m_panelRect.right
		&& m_panelRect.top <= pos.y && pos.y <= m_panelRect.bottom)
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
	const auto pageCount = wxGetApp().GetPageCount();
	if ( pageCount == 0 )
		return;

	const auto maxValue = pageCount - 1;
	if(maxValue == 0)
		return;

	const wxPoint pos = evt.GetPosition();
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
			scrollEvent.ResumePropagation(wxEVENT_PROPAGATE_MAX);
			ProcessWindowEvent(scrollEvent);
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
}

void ComicZipViewerFrame::TryRender()
{
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
	for(auto& pair: m_iconBitmapTable)
	{
		auto& bundle = std::get<0>(pair.second);
		auto image = bundle.GetBitmapFor(this).ConvertToImage();
		auto size = image.GetSize();
		HRESULT hr;
		ComPtr<ID2D1Bitmap1> bitmap;
		ComPtr<ID3D11Texture2D> texture2d;
		ComPtr<IDXGISurface> surface;
		D3D11_TEXTURE2D_DESC texture2dDesc{};
		texture2dDesc.ArraySize = 1;
		texture2dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		texture2dDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texture2dDesc.MipLevels = 1;
		texture2dDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		texture2dDesc.Usage = D3D11_USAGE_DYNAMIC;
		texture2dDesc.SampleDesc.Count = 1;
		texture2dDesc.Width = size.GetWidth();
		texture2dDesc.Height = size.GetHeight();
		hr = m_d3dDevice->CreateTexture2D(&texture2dDesc, nullptr, &texture2d);
		hr = texture2d.As(&surface);
		hr = m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);
		if(FAILED(hr))
		{
			wxLogError(wxS("Failed to create D2D Bitmap"));
			return;
		}

		D3D11_MAPPED_SUBRESOURCE mappedRect;
		m_d3dContext->Map(texture2d.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRect);
		const wxByte* const rowData = image.GetData();
		const wxByte* const alpha = image.GetAlpha();
		const bool hasAlpha = image.HasAlpha();
		for (int row = 0; row < size.GetHeight(); ++row)
		{
			for (int cols = 0; cols < size.GetWidth(); ++cols)
			{
				const int idx = row * 3 * size.GetWidth() + cols * 3;
				uint32_t rgba = 0;
				rgba = rowData[idx] | rowData[idx + 1] << 8 | rowData[idx + 2] << 16 | (hasAlpha ? alpha[row * size.GetWidth() + cols] : 0xFF) << 24;
				*reinterpret_cast<uint32_t*>((uint8_t*)mappedRect.pData + row * mappedRect.RowPitch + cols * 4) = rgba;
			}
		}

		m_d3dContext->Unmap(texture2d.Get(), 0);
		std::get<1>(pair.second) = bitmap;
	}
}

void ComicZipViewerFrame::SetSeekBarPos(int value)
{
	m_valueSeekBar = value;
	m_offsetSeekbarThumbPos = std::nullopt;
	TryRender();
}

void ComicZipViewerFrame::SetImageResizeMode(ImageSizeMode mode)
{
	m_imageSizeMode = mode;
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
	EVT_RIGHT_DOWN(ComicZipViewerFrame::OnRButtonDown)
	EVT_RIGHT_UP(ComicZipViewerFrame::OnRButtonUp)
	EVT_LEFT_DOWN(ComicZipViewerFrame::OnLMouseDown)
	EVT_LEFT_UP(ComicZipViewerFrame::OnLMouseUp)
	EVT_MOTION(ComicZipViewerFrame::OnMouseMove)
	EVT_LEAVE_WINDOW(ComicZipViewerFrame::OnMouseLeave)
	EVT_COMMAND(wxID_ANY, wxEVT_SHOW_CONTROL_PANEL, ComicZipViewerFrame::OnShowControlPanel)
	EVT_COMMAND(wxID_ANY, wxEVT_HIDE_CONTROL_PANEL, ComicZipViewerFrame::OnHideControlPanel)
	EVT_DPI_CHANGED(ComicZipViewerFrame::OnDpiChanged)
END_EVENT_TABLE()
