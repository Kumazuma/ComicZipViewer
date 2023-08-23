#pragma once
#include "framework.h"
#include <wx/wx.h>
#include <wx/treectrl.h>
#include <d2d1_3.h>

class BookmarksDialog: public wxDialog
{
	wxDECLARE_EVENT_TABLE();

public:
	BookmarksDialog() = default;
	bool Create(wxWindow* parent, wxWindowID winId, wxEvtHandler* pView, std::vector<std::tuple<int, wxString, wxString>>&& list);
	WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) override;
	int GetSelection() const;

protected:
	void OnPaintEvent(wxPaintEvent&);
	void OnSizeEvent(wxSizeEvent&);
	void UpdateClientSize(const wxSize& size);
	void ResizeSwapChain(const wxSize& size);
	void Render();
	void OnTreeItemActivated(wxTreeEvent&);
	void OnTreeSelectionChanged(wxTreeEvent&);
private:
	wxTreeCtrl* m_pTreeCtrl;
	std::vector<std::tuple<int, wxString, wxString>> m_list;
	wxEvtHandler* m_pView;
	bool m_isSizing;
	bool m_onPainting;
	int m_selectedIdx;
	wxBitmap m_backBuffer;
	wxSize m_clientSize;
};
