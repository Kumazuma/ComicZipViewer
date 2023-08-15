#pragma once
#include "framework.h"
#include <wx/wx.h>
#include <d2d1_3.h>

class BookmarksDialog: public wxDialog
{
	wxDECLARE_EVENT_TABLE();

public:
	BookmarksDialog() = default;
	bool Create(wxWindow* parent, wxWindowID winId, wxEvtHandler* pView);
	WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;

protected:
	void OnPaintEvent(wxPaintEvent&);
	void OnSizeEvent(wxSizeEvent&);
	void UpdateClientSize(const wxSize& size);
	void ResizeSwapChain(const wxSize& size);
	void Render();
private:
	wxEvtHandler* m_pView;
	bool m_isSizing;
	bool m_onPainting;
	wxBitmap m_backBuffer;
	wxSize m_clientSize;
};
