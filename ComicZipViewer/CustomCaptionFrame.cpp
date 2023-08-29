#include "framework.h"
#include "CustomCaptionFrame.h"

CustomCaptionFrame::CustomCaptionFrame()
{
}

CustomCaptionFrame::CustomCaptionFrame(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos,
	const wxSize& size, long style, const wxString& name)
{
	CustomCaptionFrame(parent , id , title , pos , size , style , name);
}

bool CustomCaptionFrame::Create(wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos,
	const wxSize& size, long style, const wxString& name)
{
	if(!D2DFrame::Create(parent, id, title, pos, size, style, name) )
	{
		return false;
	}

	return false;
}

void CustomCaptionFrame::MSWUpdateFontOnDPIChange(const wxSize& newDPI)
{
	D2DFrame::MSWUpdateFontOnDPIChange(newDPI);

}
