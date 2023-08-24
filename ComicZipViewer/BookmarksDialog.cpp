#include "framework.h"
#include "BookmarksDialog.h"
#include <wx/graphics.h>

class TreeData: public wxTreeItemData
{
public:
	TreeData(int idx)
		: wxTreeItemData()
		, m_idx(idx)
	{
		
	}

	int GetIdx() const { return m_idx; }

private:
	int m_idx;
	
};

bool BookmarksDialog::Create(wxWindow* parent, wxWindowID winId, wxEvtHandler* pView, std::vector<std::tuple<int, wxString, wxString>>&& list)
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
		auto& prefix = std::get<1>(tuple);
		wxFileName fileName(prefix);
		auto nameWithExt = fileName.GetFullName();
		auto& item = std::get<2>(tuple);
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
		id = m_pTreeCtrl->AppendItem(id, item);
		m_pTreeCtrl->SetItemData(id, new TreeData(std::get<0>(tuple)));
	}

	m_pTreeCtrl->ExpandAll();

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

void BookmarksDialog::OnTreeItemActivated(wxTreeEvent& evt)
{
	auto id = evt.GetItem();
	auto pItemData = m_pTreeCtrl->GetItemData(id);
	if(pItemData == nullptr)
		return;

	m_selectedIdx = static_cast< TreeData* >( pItemData )->GetIdx();
	this->EndModal(wxID_OK);
}

void BookmarksDialog::OnTreeSelectionChanged(wxTreeEvent& evt)
{
	m_selectedIdx = 0;
	auto id = evt.GetItem();
	if(!id.IsOk())
	{
		return;
	}

	auto pItemData = m_pTreeCtrl->GetItemData(id);
	if(pItemData == nullptr)
	{
		wxTreeItemIdValue value;
		auto childId = m_pTreeCtrl->GetFirstChild(id , value);
		if( childId.IsOk() )
		{
			m_selectedIdx = static_cast< TreeData* >( m_pTreeCtrl->GetItemData(childId) )->GetIdx();
		}

		return;
	}

	m_selectedIdx = static_cast<TreeData*>(pItemData)->GetIdx();
}

int BookmarksDialog::GetSelection() const
{
	return m_selectedIdx;
}

BEGIN_EVENT_TABLE(BookmarksDialog , wxDialog)
	//EVT_PAINT(BookmarksDialog::OnPaintEvent)
	//EVT_SIZE(BookmarksDialog::OnSizeEvent)
	EVT_TREE_ITEM_ACTIVATED(wxID_ANY, BookmarksDialog::OnTreeItemActivated)
	EVT_TREE_SEL_CHANGED(wxID_ANY, BookmarksDialog::OnTreeSelectionChanged)
END_EVENT_TABLE()
