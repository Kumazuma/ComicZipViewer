#pragma once
#include "framework.h"
#include <wx/wx.h>
#include <wrl.h>
#undef GetHwnd
#include <d3d11.h>
#include <d2d1_3.h>
#include <dxgi1_3.h>
#include <optional>

class ComicZipViewerFrame: public wxFrame
{
	wxDECLARE_EVENT_TABLE();
public:
	ComicZipViewerFrame();
	~ComicZipViewerFrame() override;
	bool Create();
	WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;
	void ShowImage(const wxImage& image);
	void SetSeekBarPos(int value);
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
	void OnShowControlPanel(wxCommandEvent& event);
	void OnHideControlPanel(wxCommandEvent& event);
	void OnMouseLeave(wxMouseEvent& evt);
	void OnMouseMove(wxMouseEvent& evt);
	void OnLMouseDown(wxMouseEvent& evt);
	void OnLMouseUp(wxMouseEvent& evt);
	void OnShown(wxShowEvent& evt);
	void OnDpiChanged(wxDPIChangedEvent& event);
	void UpdateClientSize(const wxSize& sz);
private:
	ComPtr<ID3D11Device> m_d3dDevice;
	ComPtr<ID3D11DeviceContext> m_d3dContext;
	ComPtr<ID2D1Device1> m_d2dDevice;
	ComPtr<ID2D1DeviceContext1> m_d2dContext;
	ComPtr<IDXGISwapChain1> m_swapChain;
	ComPtr<ID2D1Bitmap1> m_targetBitmap;
	ComPtr<ID2D1Factory2> m_d2dFactory;
	ComPtr<ID2D1SolidColorBrush> m_d2dBlackBrush;
	ComPtr<ID2D1SolidColorBrush> m_d2dBlueBrush;
	ComPtr<ID2D1SolidColorBrush> m_d2dWhiteBrush;
	ComPtr<ID2D1Bitmap1> m_bitmap;
	bool m_isSizing;
	bool m_enterIsDown;
	bool m_shownControlPanel;
	float m_alphaControlPanel;
	std::optional<wxPoint> m_posPrevRDown;
	wxMenu* m_pContextMenu;
	wxSize m_imageSize;
	wxSize m_clientSize;
	wxRect2DDouble m_panelRect;
	std::optional<wxPoint> m_posSeekBarThumbDown;
	int m_valueSeekBar;
};

wxDECLARE_EVENT(wxEVT_SHOW_CONTROL_PANEL, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_HIDE_CONTROL_PANEL, wxCommandEvent);
