#pragma once
#include "D2DFrame.h"
class CustomCaptionFrame: public D2DFrame
{
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

protected:
	void MSWUpdateFontOnDPIChange(const wxSize& newDPI) override;
};
