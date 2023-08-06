#pragma once
#include "framework.h"
#include "PageCollection.h"
#include <wx/zipstrm.h>

class PageCollectionZip: public PageCollection
{
public:

protected:

private:
    std::unordered_map<wxString, wxZipEntry*> m_entryTable;
    std::vector<wxString> m_sorttedEntryList;
};

