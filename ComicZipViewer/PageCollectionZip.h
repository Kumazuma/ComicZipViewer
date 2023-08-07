#pragma once
#include "framework.h"
#include "PageCollection.h"
#include <wx/zipstrm.h>
#include <wx/wfstream.h>

class PageCollectionZip: public PageCollection
{
public:
	PageCollectionZip(const wxString& filePath);
	~PageCollectionZip() override;
	void GetPageNames(std::vector<wxString>* names) const override;
	IBuffer* GetPage(const wxString& pageName) override;

protected:

private:
	wxFileInputStream* m_pFileStream;
	wxZipInputStream* m_pZipStream;
	std::unordered_map<wxString, wxZipEntry*> m_entryTable;
};
