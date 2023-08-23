#pragma once
#include "framework.h"
#include <wx/wx.h>
#include <wrl.h>
#undef GetHwnd
#include <d3d11.h>
#include <d2d1_3.h>
#include <dxgi1_3.h>
#include <optional>

struct Rect : public D2D1_RECT_F
{
	float GetWidth() const { return right - left; }
	float GetHeight() const { return bottom - top; }

};

enum class ImageViewModeKind
{
	ORIGINAL,
	FIT_PAGE,
	FIT_WIDTH
};

constexpr wxWindowID ID_BTN_FIT_WIDTH = wxID_HIGHEST + 1;
constexpr wxWindowID ID_BTN_FIT_PAGE = ID_BTN_FIT_WIDTH + 1;
constexpr wxWindowID ID_BTN_ORIGINAL =ID_BTN_FIT_PAGE + 1;
constexpr wxWindowID ID_BTN_BOOKMARK_VIEW = ID_BTN_ORIGINAL + 1;
constexpr wxWindowID ID_BTN_ADD_MARK = ID_BTN_BOOKMARK_VIEW + 1;

class ComicZipViewerFrame: public wxFrame
{
	wxDECLARE_EVENT_TABLE();
public:
	ComicZipViewerFrame();
	~ComicZipViewerFrame() override;
	bool Create(wxEvtHandler* pView);
	WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;
	void ShowImage(const wxImage& image);
	void SetSeekBarPos(int value);
	void SetImageViewMode(ImageViewModeKind mode);
	void SetPageIsMarked(bool);
protected:
	void DoThaw() override;
	void OnSize(wxSizeEvent& evt);
	void OnKeyDown(wxKeyEvent& evt);
	void OnKeyUp(wxKeyEvent& evt);
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
	void TryRender();
	void GenerateIconBitmaps();
	void OnContextMenu(wxContextMenuEvent& evt);
	void UpdateScaledImageSize();
	void OnMouseWheel(wxMouseEvent& evt);
	void ScrollImageVertical(int delta);
private:
	wxEvtHandler* m_pView;
	wxBitmapBundle m_iconFitPage;
	wxBitmapBundle m_iconFitWidth;
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
	ComPtr<ID2D1SolidColorBrush> m_d2dBackGroundWhiteBrush;
	ComPtr<ID2D1SolidColorBrush> m_d2dGrayBrush;
	ComPtr<ID2D1StrokeStyle> m_d2dSimpleStrokeStyle;
	ComPtr<ID2D1Bitmap1> m_bitmap;
	ComPtr<ID2D1Layer> m_controlPanelLayer;
	ComPtr<ID2D1Bitmap1> m_iconAtlas;
	std::unordered_map<std::wstring_view, std::tuple<wxBitmapBundle, D2D1_RECT_F>> m_iconBitmapInfo;
	bool m_isSizing;
	bool m_enterIsDown;
	bool m_shownControlPanel;
	float m_alphaControlPanel;
	wxMenu* m_pContextMenu;
	wxSize m_imageSize;
	wxSize m_clientSize;
	Rect m_panelRect;
	Rect m_seekBarRect;
	Rect m_fitWidthBtnRect;
	Rect m_fitPageBtnRect;
	Rect m_originalBtnRect;
	Rect m_bookmarkViewBtnRect;
	Rect m_addMarkBtnRect;
	std::optional<wxPoint> m_offsetSeekbarThumbPos;
	int m_valueSeekBar;
	bool m_willRender;
	bool m_pageIsMarked;
	wxWindowID m_idMouseOver;
	ImageViewModeKind m_imageViewMode;
	wxWindowID m_latestHittenButtonId;
	D2D1_SIZE_F m_scaledImageSize;
	D2D1_SIZE_F m_movableCenterRange;
	D2D1_POINT_2F m_center;
};

wxDECLARE_EVENT(wxEVT_SHOW_CONTROL_PANEL, wxCommandEvent);
wxDECLARE_EVENT(wxEVT_HIDE_CONTROL_PANEL, wxCommandEvent);
