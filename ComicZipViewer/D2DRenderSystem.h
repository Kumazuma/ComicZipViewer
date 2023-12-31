#pragma once
#include "framework.h"
#include <wx/wx.h>
#include <d2d1_3.h>
#include <d3d11_4.h>

class D2DRenderSystem
{
public:
	HRESULT Initalize(wxWindow* pWindow, const wxSize& initSwapChainSize);
	HRESULT GetD2DDevice(ID2D1Device** ppDevice);
	HRESULT GetD2DDeviceContext(ID2D1DeviceContext1** ppDeviceContext);
	HRESULT GetD3DDevice(ID3D11Device** ppDevice);
	HRESULT GetD3DDeviceContext(ID3D11DeviceContext** ppDeviceContext);
	HRESULT GetSwapChain(IDXGISwapChain1** ppSwapChain);
	HRESULT GetD2DTargetBitmap(ID2D1Bitmap1** ppTargetBitmap);
	HRESULT GetD2DFactory(ID2D1Factory2** ppFactory);
	HRESULT ResizeSwapChain(const wxSize& size);
	HRESULT GetSolidColorBrush(const D2D1::ColorF& color , ID2D1SolidColorBrush** ppBrush);

private:
	ComPtr<ID3D11Device> m_d3dDevice;
	ComPtr<ID3D11DeviceContext> m_d3dContext;
	ComPtr<ID2D1Device1> m_d2dDevice;
	ComPtr<ID2D1DeviceContext1> m_d2dContext;
	ComPtr<IDXGISwapChain1> m_swapChain;
	ComPtr<ID2D1Bitmap1> m_targetBitmap;
	ComPtr<ID2D1Factory2> m_d2dFactory;
	std::unordered_map<UINT , ComPtr<ID2D1SolidColorBrush>> m_solidBrushCacheTable;
};
