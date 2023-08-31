#pragma once
#include "PageCollection.h"
#include <wx/wfstream.h>
#include <wx/tarstrm.h>

class PageCollectionCbt:
    public PageCollection
{
public:
	PageCollectionCbt(const wxString& filePath);
	~PageCollectionCbt() override;
	void GetPageNames(std::vector<wxString>* names) const override;
	IBuffer* GetPage(const wxString& pageName) override;

protected:

private:
	wxFileInputStream* m_pFileStream;
	wxTarInputStream* m_pCbtStream;
	std::unordered_map<wxString, wxTarEntry*> m_entryTable;
};

