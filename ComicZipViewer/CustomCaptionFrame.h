#pragma once
#include "D2DRenderSystem.h"

class CustomCaptionFrame: public wxFrame
{
	static constexpr int FAKE_SHADOW_HEIGHT = 1;
	static constexpr int MAXIMIZED_RECTANGLE_OFFSET = 3;
	static constexpr wxWindowID wxID_MINIMIZE_BOX = wxID_HIGHEST + 1;
	static constexpr wxWindowID wxID_MAXIMIZE_BOX = wxID_MINIMIZE_BOX + 1;
	static constexpr wxWindowID wxID_CLOSE_BOX = wxID_MAXIMIZE_BOX + 1;
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

protected:
	void MSWUpdateFontOnDPIChange(const wxSize& newDPI) override;

	void Render();
	void TryRender();
	void DeferredRender();
	void RenderCaption();
	virtual void DoRender() {}
	virtual void DoThaw() override;

private:
	WXLRESULT OnNcCalcSize(UINT nMsg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcHitTest(UINT nMsg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcLButtonDown(UINT nMsg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcLButtonUp(UINT nMSg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcMouseMove(UINT nMsg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcMouseLeave(UINT nMsg, WPARAM wParam, LPARAM lParam);
	WXLRESULT OnNcMouseEnter(UINT nMsg, WPARAM wParam, LPARAM lParam);
	void UpdateCaptionDesc(int dpi);
private:
	D2DRenderSystem m_renderSystem;
	wxWindowID m_currentHoveredButtonId;
	int m_borderPadding;
	bool m_willRender;
	wxSize m_borderThickness;
	wxRect m_titleBarRect;
	struct
	{
		wxRect close;
		wxRect maximize;
		wxRect minimize;
	} m_titleBarButtonRects;
};
