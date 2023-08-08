#include "framework.h"
#include <wx/stream.h>
#include "PageCollectionZip.h"
#include "SimpleBuffer.h"

PageCollectionZip::PageCollectionZip(const wxString& filePath)
	: m_pFileStream(nullptr)
	, m_pZipStream(nullptr)
{
	m_pFileStream = new wxFileInputStream(filePath);
	if(!m_pFileStream->IsOk())
		return;

	m_pZipStream =  new wxZipInputStream(*m_pFileStream);
	if(!m_pZipStream->IsOk())
		return;

	wxZipEntry* entry;
	while((entry = m_pZipStream->GetNextEntry()) != nullptr)
	{
		wxString name = entry->GetName();
		m_entryTable.emplace(name, entry);
	}

	m_pageCount = m_entryTable.size();
}

PageCollectionZip::~PageCollectionZip()
{
	delete m_pFileStream;
	delete m_pZipStream;
	m_pZipStream = nullptr;
	m_pFileStream = nullptr;
	for(auto& entry: m_entryTable)
	{
		delete entry.second;
	}
}

void PageCollectionZip::GetPageNames(std::vector<wxString>* names) const
{
	for(auto& pair: m_entryTable)
	{
		names->push_back(pair.first);
	}
}

IBuffer* PageCollectionZip::GetPage(const wxString& pageName)
{
	auto it = m_entryTable.find(pageName);
	if(it == m_entryTable.end())
		return nullptr;

	auto entry = it->second;
	if(!m_pZipStream->OpenEntry(*entry))
		return nullptr;

	auto* pSimpleBuffer = new SimpleBuffer(entry->GetSize());
	m_pZipStream->ReadAll(pSimpleBuffer ->GetPointer(), pSimpleBuffer ->GetLength());
	m_pZipStream->CloseEntry();

	return pSimpleBuffer;
}
