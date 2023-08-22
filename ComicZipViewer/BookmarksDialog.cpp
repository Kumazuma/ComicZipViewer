#include "framework.h"
#include "BookmarksDialog.h"
#include <wx/graphics.h>

bool BookmarksDialog::Create(wxWindow* parent, wxWindowID winId, wxEvtHandler* pView, std::vector<std::tuple<wxString, wxString>>&& list)
{
	m_list = std::move(list);
	m_pView = pView;
	wxDialog::Create(parent, winId, wxS("Bookmarks"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);
	m_pTreeCtrl = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT);
	auto bSizer = new wxBoxSizer(wxVERTICAL);
	bSizer->Add(m_pTreeCtrl, 1, wxEXPAND | wxALL, 5);
	bSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 5);
	auto rootId = m_pTreeCtrl->AddRoot(wxS(""));

	std::unordered_map<wxString, wxTreeItemId> roots; // <prefix>
	std::unordered_map<wxString, wxString> prefixTreeTextTable; // <NameWithExt, Prefix>
	for(auto& tuple : m_list)
	{
		wxTreeItemId id;
		auto& prefix = std::get<0>(tuple);
		wxFileName fileName(prefix);
		auto nameWithExt = fileName.GetFullName();
		auto& item = std::get<1>(tuple);
		auto rootTableIt = roots.find(prefix);
		if(rootTableIt == roots.end())
		{
			auto it2 = prefixTreeTextTable.find(nameWithExt);
			wxString text;
			if(it2 == prefixTreeTextTable.end())
			{
				prefixTreeTextTable.emplace(nameWithExt, prefix);
				text = nameWithExt;
			}
			else
			{
				text = wxString::Format(wxS("%s(%s)"), fileName.GetFullName(), fileName.GetPath());
				if(!it2->second.IsEmpty())
				{
					auto& prefix2 = it2->second;
					fileName = wxFileName(prefix2);
					id = roots[it2->second];
					auto newName = wxString::Format(wxS("%s(%s)"), fileName.GetFullName(), fileName.GetPath());
					it2->second.Clear();
					m_pTreeCtrl->SetItemText(id, newName);
				}
			}

			id = m_pTreeCtrl->AppendItem(rootId, text);
			rootTableIt = roots.emplace(prefix, id).first;
		}

		id = rootTableIt->second;
		m_pTreeCtrl->AppendItem(id, item);
	}

	SetSizer(bSizer);
	Layout();
	return true;
}

WXLRESULT BookmarksDialog::MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam)
{
	switch(message)
	{
	case WM_ENTERSIZEMOVE:
		m_isSizing = true;
		break;
	case WM_EXITSIZEMOVE:
		m_isSizing = false;
		//UpdateClientSize(GetClientSize());
		//ResizeSwapChain(m_clientSize);
		//Render();
		//{
		//	wxClientDC dc(this);
		//	wxMemoryDC memDC(m_backBuffer);
		//	dc.Blit(wxPoint(0, 0), m_backBuffer.GetSize(), &memDC, wxPoint(0, 0));
		//}
		break;
	}

	return wxDialog::MSWWindowProc(message , wParam , lParam);
}

void BookmarksDialog::OnPaintEvent(wxPaintEvent&)
{
	m_onPainting = true;
	wxPaintDC paintDC(this);
	wxMemoryDC memDC(m_backBuffer);
	paintDC.Blit(wxPoint(0, 0), m_backBuffer.GetSize(), &memDC, wxPoint(0, 0));
	m_onPainting = false;
}

void BookmarksDialog::OnSizeEvent(wxSizeEvent&)
{
	if(m_isSizing)
		return;

	UpdateClientSize(GetClientSize());
	ResizeSwapChain(m_clientSize);
	Render();
}

void BookmarksDialog::UpdateClientSize(const wxSize& size)
{
	m_clientSize = size;
}

void BookmarksDialog::ResizeSwapChain(const wxSize& size)
{
	m_backBuffer = wxBitmap(size.x, size.y);
}

void BookmarksDialog::Render()
{
	if(IsFrozen())
		return;

	wxMemoryDC memDC(m_backBuffer);
	auto d2dContext = wxGraphicsRenderer::GetDirect2DRenderer();
	auto context = d2dContext->CreateContext(memDC);
	context->ClearRectangle(0.0, 0.0, m_clientSize.x, m_clientSize.y);
	delete context;
}

BEGIN_EVENT_TABLE(BookmarksDialog , wxDialog)
//EVT_PAINT(BookmarksDialog::OnPaintEvent)
//EVT_SIZE(BookmarksDialog::OnSizeEvent)
END_EVENT_TABLE()
