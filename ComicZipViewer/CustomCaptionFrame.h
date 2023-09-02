#pragma once
#include "D2DRenderSystem.h"
#include "UpdateSystem.h"
#include <dwrite_3.h>

class CustomCaptionFrame: public wxFrame
{
	static constexpr int FAKE_SHADOW_HEIGHT = 1;
	static constexpr float MAXIMIZED_RECTANGLE_OFFSET = 1.5f;
	static constexpr wxWindowID wxID_MINIMIZE_BOX = wxID_HIGHEST + 1;
	static constexpr wxWindowID wxID_MAXIMIZE_BOX = wxID_MINIMIZE_BOX + 1;
	static constexpr wxWindowID wxID_CLOSE_BOX = wxID_MAXIMIZE_BOX + 1;
	static constexpr wxWindowID wxID_FULLSCREEN_BOX = wxID_CLOSE_BOX + 1;
	class ShowWithFadeInWorker;
	class HideWithFadeOutWorker;
public:
	CustomCaptionFrame();
	CustomCaptionFrame(wxWindow* parent ,
			wxWindowID id ,
			const wxString& title ,
			const wxPoint& pos = wxDefaultPosition ,
			const wxSize& size = wxDefaultSize ,
			long style = wxDEFAULT_FRAME_STYLE ,
			const wxString& name = wxASCII_STR(wxFrameNameStr));

	bool Create(wxWindow* parent ,
			wxWindowID id ,
			const wxString& title ,
			const wxPoint& pos = wxDefaultPosition ,
			const wxSize& size = wxDefaultSize ,
			long style = wxDEFAULT_FRAME_STYLE ,
			const wxString& name = wxASCII_STR(wxFrameNameStr));

	WXLRESULT MSWWindowProc(WXUINT nMsg, WXWPARAM wParam, WXLPARAM lParam) override;
	
	void Render();
	void TryRender();

protected:
	void MSWUpdateFontOnDPIChange(const wxSize& newDPI) override;

	void DeferredRender();
	void RenderCaption();
	virtual void DoRender() {}
	virtual void DoThaw() override;
	void ResizeSwapChain(wxSize& size);
	HRESULT GetD2DDeviceContext(ID2D1DeviceContext1** ppContext);
	HRESULT GetD3DDeviceContext(ID3D11DeviceContext** ppContext);
	HRESULT GetD3DDevice(ID3D11Device** ppDevice);
	HRESULT GetContainingOutput(IDXGIOutput** ppOutput);
private:
	WXLRESULT OnNcCalcSize(UINT nMsg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcHitTest(UINT nMsg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcLButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcLButtonUp(UINT nMSg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcMouseMove(UINT nMsg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcMouseLeave(UINT nMsg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcMouseEnter(UINT nMsg, WPARAM wParam, LPARAM lParam);
	void UpdateCaptionDesc(int dpi);
	wxWindowID GetMouseOveredButtonId(const wxPoint& cursor) const;
private:
	D2DRenderSystem m_renderSystem;
	wxWindowID m_currentHoveredButtonId;
	wxWindowID m_buttonDownedId;
	int m_borderPadding;
	bool m_willRender;
	wxSize m_borderThickness;
	wxRect m_titleBarRect;
	struct
	{
		wxRect close;
		wxRect maximize;
		wxRect minimize;
		wxRect fullScreen;
	} m_titleBarButtonRects;
	uint32_t m_captionOpacity;
	float m_titleTextSize;
	bool m_shownCaption;
	ComPtr<IDWriteTextFormat> m_dwTextFormat;
	ComPtr<IDWriteFactory> m_dwFactory;
};
