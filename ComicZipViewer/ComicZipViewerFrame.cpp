#include "framework.h"
#include "ComicZipViewerFrame.h"
#include "ComicZipViewer.h"
#include <wx/bitmap.h>

wxDEFINE_EVENT(wxEVT_SHOW_CONTROL_PANEL, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_HIDE_CONTROL_PANEL, wxCommandEvent);

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxgi.lib")

ComicZipViewerFrame::ComicZipViewerFrame()
	: m_isSizing(false)
	, m_enterIsDown(false)
	, m_pContextMenu(nullptr)
	, m_alphaControlPanel(0.f)
	, m_shownControlPanel(false)
	, m_posSeekBarThumbDown()
	, m_valueSeekBar()
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
	m_pContextMenu = new wxMenu();
	m_pContextMenu->Append(wxID_OPEN, wxS("&Open"));
	m_pContextMenu->Append(wxID_CLOSE, wxS("&Close"));
	if (!ret)
	{
		return false;
	}
	wxBitmapBundle a;
	HRESULT hRet;
	HWND hWnd = GetHWND();
	D3D_FEATURE_LEVEL featureLevel;
	hRet = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &m_d3dDevice, &featureLevel, &m_d3dContext);
	if(FAILED(hRet))
	{
		wxLogError(wxS("Failed to create d3d device"));
		return false;
	}

	UpdateClientSize(GetClientSize());

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

	ComPtr<IDXGIFactory2> dxgiFactory = nullptr;
	CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, __uuidof(IDXGIFactory2),  &dxgiFactory);
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
	m_d2dContext->SetTarget(m_targetBitmap.Get());
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
	HRESULT hr;
	ComPtr<ID3D11Texture2D> texture2d;
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
	ComPtr<IDXGISurface> surface;
	ComPtr<ID2D1Bitmap1> bitmap;
	hr = texture2d.As(&surface);
	hr = m_d2dContext->CreateBitmapFromDxgiSurface(surface.Get(), D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE, D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)), &bitmap);
	if(FAILED(hr))
	{
		wxLogError(wxS("Failed to create D2D Bitmap"));
		return;
	}

	D2D1_MAPPED_RECT mappedRect;
	bitmap->Map(D2D1_MAP_OPTIONS_DISCARD | D2D1_MAP_OPTIONS_WRITE, &mappedRect);
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
			*reinterpret_cast<uint32_t*>(mappedRect.bits + row * mappedRect.pitch + cols * 4) = rgba;
		}
	}

	bitmap->Unmap();
	m_bitmap = bitmap;
	m_imageSize = size;
	Render();
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
	HRESULT hRet;
	m_d2dContext->BeginDraw();
	m_d2dContext->Clear(D2D1::ColorF(1.f, 0.f, 1.f, 1.f));
	if(m_bitmap)
	{
		m_d2dContext->DrawBitmap(m_bitmap.Get(), D2D1::RectF(0.f, 0.f, m_imageSize.GetWidth(), m_imageSize.GetHeight()));
	}

	if(m_alphaControlPanel > 0.f)
	{
		const float scale = GetDPIScaleFactor();
		const auto& size = m_clientSize;
		m_d2dContext->PushLayer(D2D1::LayerParameters1(D2D1::InfiniteRect(), nullptr, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE, D2D1::IdentityMatrix(), m_alphaControlPanel), nullptr);
		m_d2dContext->FillRectangle(D2D1::RectF(m_panelRect.GetLeft(), m_panelRect.GetTop(), m_panelRect.GetRight(), m_panelRect.GetBottom()), m_d2dBlackBrush.Get());
		m_d2dContext->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(m_panelRect.GetLeft() + 15.f * scale , m_panelRect.GetTop() + 15.f * scale , m_panelRect.GetRight() - 15.f * scale , m_panelRect.GetTop() + 15.f * scale + 5.f * scale) , 2.5f * scale , 2.5f * scale) , m_d2dBlueBrush.Get());

		if( wxGetApp().GetPageCount() != 0 )
		{
			const float percent = m_valueSeekBar / ( float ) wxGetApp().GetPageCount();
			const float bar = ( ( m_panelRect.GetRight() - 15.f * scale - 2.5f * scale ) - ( m_panelRect.GetLeft() + 15.f * scale + 2.5f * scale ) ) * percent + 2.5f * scale + m_panelRect.GetLeft() + 15.f * scale;

			m_d2dContext->FillEllipse(D2D1::Ellipse(D2D1::Point2F(bar , m_panelRect.GetTop() + 15.f * scale + 2.5f * scale) , 10.f * scale , 10.f * scale) , m_d2dWhiteBrush.Get());
			m_d2dContext->FillEllipse(D2D1::Ellipse(D2D1::Point2F(bar , m_panelRect.GetTop() + 15.f * scale + 2.5f * scale) , 5.0f * scale , 5.0f * scale) , m_d2dBlueBrush.Get());
		}

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

		Render();
		int64_t current = GetTick();
		const int64_t diff = current - latestTick;
		latestTick = current;
		float delta = ((diff * 4096ll) / frequency) * ( 1.f / 4096.f);
		m_alphaControlPanel += delta * 48.f;

		wxYield();
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

		Render();
		int64_t current = GetTick();
		const int64_t diff = current - latestTick;
		latestTick = current;
		float delta = ((diff * 4096ll) / frequency) * ( 1.f / 4096.f);
		m_alphaControlPanel -= delta * 48.f;
		wxYield();
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
	if(m_posSeekBarThumbDown.has_value())
	{
		if(evt.LeftIsDown() )
		{
			const float scale = GetDPIScaleFactor();
			auto& prevPos = *m_posSeekBarThumbDown;
			const int diff = pos.x - prevPos.x;
			const float percent = diff / ( ( m_panelRect.GetRight() - 15.f * scale - 2.5f * scale ) - ( m_panelRect.GetLeft() + 15.f * scale + 2.5f * scale ) );
			const float gap = ( ( m_panelRect.GetRight() - 15.f * scale - 2.5f * scale ) - ( m_panelRect.GetLeft() + 15.f * scale + 2.5f * scale ) ) / wxGetApp().GetPageCount();
			const int newValue = ( int ) floor(percent * wxGetApp().GetPageCount());
			if ( newValue != 0 )
			{
				prevPos.x += pos.x - newValue * gap;
				char buff[ 80 ];
				sprintf_s(buff , "prev: %d, diff: %d\n" , m_valueSeekBar , newValue);
				OutputDebugStringA(buff);
				m_valueSeekBar += newValue;
				if ( m_valueSeekBar < 0 )
				{
					m_valueSeekBar = 0;
				}
				else if(m_valueSeekBar >= wxGetApp().GetPageCount())
				{
					m_valueSeekBar = wxGetApp().GetPageCount() - 1;
				}
					
				Render();
				auto scrollEvent = new wxScrollEvent(wxEVT_SCROLL_THUMBTRACK , GetId() , m_valueSeekBar , wxHORIZONTAL);
				scrollEvent->SetEventObject(this);
				QueueEvent(scrollEvent);
			}

			return;
		}

		m_posSeekBarThumbDown = std::nullopt;
	}

	if(m_panelRect.Contains(pos))
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
	m_posSeekBarThumbDown = std::nullopt;
	const auto pageCount = wxGetApp().GetPageCount();
	if ( pageCount == 0 )
		return;

	const float scale = GetDPIScaleFactor();
	const float percent = m_valueSeekBar / ( float ) pageCount;
	const float x = ( ( m_panelRect.GetRight() - 15.f * scale - 2.5f * scale ) - ( m_panelRect.GetLeft() + 15.f * scale + 2.5f * scale ) ) * percent + 2.5f * scale + m_panelRect.GetLeft() + 15.f * scale;
	const float y = m_panelRect.GetTop() + 15.f * scale + 2.5f * scale;
	const float radius = 10.f * scale;
	const wxPoint pos = evt.GetPosition();
	const float diffX = pos.x - x;
	const float diffY = pos.y - y;
	if(diffX * diffX + diffY * diffY <= radius * radius)
	{
		m_posSeekBarThumbDown = pos;
	}
}

void ComicZipViewerFrame::OnLMouseUp(wxMouseEvent& evt)
{
	m_posSeekBarThumbDown = std::nullopt;
}

void ComicZipViewerFrame::OnShown(wxShowEvent& evt)
{

}

void ComicZipViewerFrame::OnDpiChanged(wxDPIChangedEvent& event)
{

}

void ComicZipViewerFrame::UpdateClientSize(const wxSize& sz)
{
	m_clientSize = sz;
	const float scale = GetDPIScaleFactor();
	const float xywh[ 4 ]
	{
		5 * scale,
		sz.y - 5 * scale - 150 * scale,
		sz.x - 10 * scale,
		150 * scale
	};

	m_panelRect.m_x = xywh[ 0 ];
	m_panelRect.m_y = xywh[ 1 ];
	m_panelRect.m_width = xywh[ 2 ];
	m_panelRect.m_height = xywh[ 3 ];
}

void ComicZipViewerFrame::SetSeekBarPos(int value)
{
	m_valueSeekBar = value;
	m_posSeekBarThumbDown = std::nullopt;
	Render();
}

BEGIN_EVENT_TABLE(ComicZipViewerFrame, wxFrame)
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
END_EVENT_TABLE()
