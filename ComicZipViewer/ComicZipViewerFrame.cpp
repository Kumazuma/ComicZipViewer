#include "framework.h"
#include "ComicZipViewerFrame.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxgi.lib")

ComicZipViewerFrame::ComicZipViewerFrame()
	: m_isSizing(false)
	, m_enterIsDown(false)
	, m_pContextMenu(nullptr)
{
}

ComicZipViewerFrame::~ComicZipViewerFrame()
{
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

	HRESULT hRet;
	HWND hWnd = GetHWND();
	D3D_FEATURE_LEVEL featureLevel;
	hRet = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &m_d3dDevice, &featureLevel, &m_d3dContext);
	if(FAILED(hRet))
	{
		wxLogError(wxS("Failed to create d3d device"));
		return false;
	}

	auto clientSize = GetSize();

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc1{};
	swapChainDesc1.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc1.BufferCount = 2;
	swapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
	swapChainDesc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc1.Height = clientSize.GetHeight();
	swapChainDesc1.Width = clientSize.GetWidth();
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

	hRet = m_d2dContext->CreateSolidColorBrush(D2D1::ColorF(1.f, 0.f, 0.f), &m_d2dBrush);

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
		ResizeSwapChain(GetClientSize());
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
	wxByte* const rowData = image.GetData();
	for(int row = 0; row < size.GetHeight(); ++row)
	{
		for(int cols = 0; cols < size.GetWidth(); ++cols)
		{
			const int idx = row * 3 * size.GetWidth() + cols * 3;
			uint32_t rgba = 0;
			rgba = rowData[idx] | rowData[idx + 1] << 8 | rowData[idx + 2] << 16 | 0xFF << 24;
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

	ResizeSwapChain(evt.GetSize());
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

	hRet = m_d2dContext->EndDraw();
	m_d3dContext->Flush();
	hRet = m_swapChain->Present(0, 0);
}

void ComicZipViewerFrame::Fullscreen()
{
	ShowFullScreen(true);
	ComPtr<IDXGIOutput> output;
	m_swapChain->GetContainingOutput(&output);
	DXGI_OUTPUT_DESC desc{};
	output->GetDesc(&desc);
	RECT& coordenates = desc.DesktopCoordinates;
	ResizeSwapChain(wxSize(coordenates.right - coordenates.left, coordenates.bottom - coordenates.top));
	Render();
}

void ComicZipViewerFrame::RestoreFullscreen()
{
	ShowFullScreen(false);
	ResizeSwapChain(GetClientSize());
	Render();
}

BEGIN_EVENT_TABLE(ComicZipViewerFrame, wxFrame)
EVT_SIZE(ComicZipViewerFrame::OnSize)
EVT_KEY_DOWN(ComicZipViewerFrame::OnKeyDown)
EVT_KEY_UP(ComicZipViewerFrame::OnKeyUp)
EVT_RIGHT_DOWN(ComicZipViewerFrame::OnRButtonDown)
EVT_RIGHT_UP(ComicZipViewerFrame::OnRButtonUp)
END_EVENT_TABLE()
