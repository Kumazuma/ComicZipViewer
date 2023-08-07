#pragma once
#include "framework.h"
#include <wx/wx.h>
#include <wrl.h>
#undef GetHwnd
#include <d3d11.h>
#include <d2d1_3.h>
#include <dxgi1_3.h>

class ComicZipViewerFrame: public wxFrame
{
	wxDECLARE_EVENT_TABLE();
public:
	ComicZipViewerFrame();
	~ComicZipViewerFrame() override;
	bool Create();
	WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;
	void ShowImage(const wxImage& image);
protected:
	void OnSize(wxSizeEvent& evt);
	void OnKeyDown(wxKeyEvent& evt);
	void OnKeyUp(wxKeyEvent& evt);
	void OnRButtonDown(wxMouseEvent& evt);
	void OnRButtonUp(wxMouseEvent& evt);
	void ResizeSwapChain(const wxSize clientRect);
	void Render();
	void Fullscreen();
	void RestoreFullscreen();
private:
	ComPtr<ID3D11Device> m_d3dDevice;
	ComPtr<ID3D11DeviceContext> m_d3dContext;
	ComPtr<ID2D1Device1> m_d2dDevice;
	ComPtr<ID2D1DeviceContext1> m_d2dContext;
	ComPtr<IDXGISwapChain1> m_swapChain;
	ComPtr<ID2D1Bitmap1> m_targetBitmap;
	ComPtr<ID2D1Factory2> m_d2dFactory;
	ComPtr<ID2D1SolidColorBrush> m_d2dBrush;
	ComPtr<ID2D1Bitmap1> m_bitmap;
	bool m_isSizing;
	bool m_enterIsDown;
	std::optional<wxPoint> m_posPrevRDown;
	wxMenu* m_pContextMenu;
	wxSize m_imageSize;
};
