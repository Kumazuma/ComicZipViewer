#include "framework.h"
#include "D2DRenderSystem.h"

HRESULT D2DRenderSystem::Initalize(wxWindow* pWindow, const wxSize& initSwapChainSize)
{
	auto& size = initSwapChainSize;
	HRESULT hRet;
	HWND hWnd = pWindow->GetHWND();
	ComPtr<IDXGIFactory2> dxgiFactory = nullptr;
	UINT flags = 0;
	UINT d3d11Flag = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if !defined(NDEBUG)
	flags = DXGI_CREATE_FACTORY_DEBUG;
	d3d11Flag = d3d11Flag | D3D11_CREATE_DEVICE_DEBUG;
#endif
	CreateDXGIFactory2(flags , __uuidof( IDXGIFactory2 ) , &dxgiFactory);

	D3D_FEATURE_LEVEL featureLevel;
	hRet = D3D11CreateDevice(nullptr , D3D_DRIVER_TYPE_HARDWARE , nullptr , d3d11Flag , nullptr , 0 , D3D11_SDK_VERSION , &m_d3dDevice , &featureLevel , &m_d3dContext);
	if ( FAILED(hRet) )
	{
		wxLogError(wxS("Failed to create d3d device"));
		return E_FAIL;
	}

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc1{};
	swapChainDesc1.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	swapChainDesc1.BufferCount = 3;
	swapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc1.Height = size.GetHeight();
	swapChainDesc1.Width = size.GetWidth();
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
	dxgiFactory->CreateSwapChainForHwnd(m_d3dDevice.Get() , hWnd , &swapChainDesc1 , &fullScreenDesc , nullptr , &m_swapChain);
	m_swapChain->GetParent(__uuidof( IDXGIFactory2 ) , &dxgiFactory);
	dxgiFactory->MakeWindowAssociation(hWnd , DXGI_MWA_NO_ALT_ENTER);

	ComPtr<IDXGISurface> surface;
	ComPtr<IDXGIDevice> dxgiDevice;
	m_d3dDevice.As(&dxgiDevice);
	m_swapChain->GetBuffer(0 , __uuidof( IDXGISurface ) , &surface);
	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED , __uuidof( m_d2dFactory ) , &m_d2dFactory);
	hRet = m_d2dFactory->CreateDevice(dxgiDevice.Get() , &m_d2dDevice);

	hRet = m_d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE , &m_d2dContext);
	m_d2dContext->SetDpi(96.f , 96.f);
	hRet = m_d2dContext->CreateBitmapFromDxgiSurface(
		surface.Get() ,
		D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW , D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM , D2D1_ALPHA_MODE_PREMULTIPLIED)) ,
		&m_targetBitmap);

	m_d2dContext->SetTarget(m_targetBitmap.Get());
	return S_OK;
}

HRESULT D2DRenderSystem::GetD2DDevice(ID2D1Device** ppDevice)
{
	if( ppDevice == nullptr)
	{
		return E_INVALIDARG;
	}

	if( !m_d2dDevice )
	{
		return E_FAIL;
	}

	*ppDevice = m_d2dDevice.Get();
	m_d2dDevice->AddRef();
	return S_OK;
}

HRESULT D2DRenderSystem::GetD2DDeviceContext(ID2D1DeviceContext1** ppDeviceContext)
{
	if ( ppDeviceContext == nullptr )
	{
		return E_INVALIDARG;
	}

	if ( !m_d2dContext )
	{
		return E_FAIL;
	}

	*ppDeviceContext = m_d2dContext.Get();
	m_d2dContext->AddRef();
	return S_OK;
}

HRESULT D2DRenderSystem::GetD3DDevice(ID3D11Device** ppDevice)
{
	if ( ppDevice == nullptr )
	{
		return E_INVALIDARG;
	}

	if ( !m_d3dDevice )
	{
		return E_FAIL;
	}

	*ppDevice = m_d3dDevice.Get();
	m_d3dDevice->AddRef();
	return S_OK;
}

HRESULT D2DRenderSystem::GetD3DDeviceContext(ID3D11DeviceContext** ppDeviceContext)
{
	if ( ppDeviceContext == nullptr )
	{
		return E_INVALIDARG;
	}

	if ( !m_d3dContext )
	{
		return E_FAIL;
	}

	*ppDeviceContext = m_d3dContext.Get();
	m_d3dContext->AddRef();
	return S_OK;
}

HRESULT D2DRenderSystem::GetSwapChain(IDXGISwapChain1** ppSwapChain)
{
	if ( ppSwapChain == nullptr )
	{
		return E_INVALIDARG;
	}

	if ( !m_swapChain )
	{
		return E_FAIL;
	}

	*ppSwapChain = m_swapChain.Get();
	m_swapChain->AddRef();
	return S_OK;
}

HRESULT D2DRenderSystem::GetD2DTargetBitmap(ID2D1Bitmap1** ppTargetBitmap)
{
	if ( ppTargetBitmap == nullptr )
	{
		return E_INVALIDARG;
	}

	if ( !m_targetBitmap )
	{
		return E_FAIL;
	}

	*ppTargetBitmap = m_targetBitmap.Get();
	m_targetBitmap->AddRef();
	return S_OK;
}

HRESULT D2DRenderSystem::GetD2DFactory(ID2D1Factory2** ppFactory)
{
	if ( ppFactory == nullptr )
	{
		return E_INVALIDARG;
	}

	if ( !m_d2dFactory )
	{
		return E_FAIL;
	}

	*ppFactory = m_d2dFactory.Get();
	m_d2dFactory->AddRef();
	return S_OK;
}

HRESULT D2DRenderSystem::ResizeSwapChain(const wxSize& size)
{
	HRESULT hRet;
	ComPtr<IDXGISurface> surface;
	m_d2dContext->SetTarget(nullptr);
	m_targetBitmap.Reset();
	m_swapChain->ResizeBuffers(0 , size.GetWidth() , size.GetHeight() , DXGI_FORMAT_UNKNOWN , 0);
	m_swapChain->GetBuffer(0 , __uuidof( IDXGISurface ) , &surface);
	hRet = m_d2dContext->CreateBitmapFromDxgiSurface(
		surface.Get() ,
		D2D1::BitmapProperties1(
			D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW ,
			D2D1::PixelFormat(DXGI_FORMAT_R8G8B8A8_UNORM , D2D1_ALPHA_MODE_PREMULTIPLIED)) ,
		&m_targetBitmap);

	m_d2dContext->SetTarget(m_targetBitmap.Get());
	return S_OK;
}

HRESULT D2DRenderSystem::GetSolidColorBrush(const D2D1::ColorF& color, ID2D1SolidColorBrush** ppBrush)
{
	UINT key = 0;
	key |= static_cast< int >( color.r * 255.f ) % 255;
	key = key << 8;
	key |= static_cast< int >( color.g * 255.f ) % 255;
	key = key << 8;
	key |= static_cast< int >( color.b * 255.f ) % 255;
	key = key << 8;
	key |= static_cast< int >( color.a * 255.f ) % 255;
	key = key << 8;

	auto it = m_solidBrushCacheTable.find(key);
	if(it == m_solidBrushCacheTable.end())
	{
		ComPtr<ID2D1SolidColorBrush> newBrush;
		m_d2dContext->CreateSolidColorBrush(color , &newBrush);
		it = m_solidBrushCacheTable.emplace(key , std::move(newBrush)).first;
	}

	const auto& brush = it->second;
	*ppBrush = brush.Get();
	brush->AddRef();
	return S_OK;
}
