#include "framework.h"
#include "PageCollectionCbt.h"
#include "SimpleBuffer.h"

PageCollectionCbt::PageCollectionCbt(const wxString& filePath)
	: m_pFileStream(nullptr)
	, m_pCbtStream(nullptr)
{
	m_pFileStream = new wxFileInputStream(filePath);
	if(!m_pFileStream->IsOk())
		return;

	m_pCbtStream =  new wxTarInputStream(*m_pFileStream);
	if(!m_pCbtStream->IsOk())
		return;

	wxTarEntry* entry;
	while((entry = m_pCbtStream->GetNextEntry()) != nullptr)
	{
		wxString name = entry->GetName();
		m_entryTable.emplace(name, entry);
	}

	m_pageCount = m_entryTable.size();
}

PageCollectionCbt::~PageCollectionCbt()
{
	delete m_pFileStream;
	delete m_pCbtStream;
	m_pCbtStream = nullptr;
	m_pFileStream = nullptr;
	for(auto& entry: m_entryTable)
	{
		delete entry.second;
	}
}

void PageCollectionCbt::GetPageNames(std::vector<wxString>* names) const
{
	for(auto& pair: m_entryTable)
	{
		names->push_back(pair.first);
	}
}

IBuffer* PageCollectionCbt::GetPage(const wxString& pageName)
{
	auto it = m_entryTable.find(pageName);
	if(it == m_entryTable.end())
		return nullptr;

	auto entry = it->second;
	if(!m_pCbtStream->OpenEntry(*entry))
		return nullptr;

	auto* pSimpleBuffer = new SimpleBuffer(entry->GetSize());
	m_pCbtStream->ReadAll(pSimpleBuffer ->GetPointer(), pSimpleBuffer ->GetLength());
	m_pCbtStream->CloseEntry();

	return pSimpleBuffer;
}
