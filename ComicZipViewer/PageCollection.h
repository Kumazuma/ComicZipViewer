#pragma once
#include "IBuffer.h"

class PageCollection
{
public:
	virtual ~PageCollection() = default;
	int GetPageCount() const { return m_pageCount; }
	virtual void GetPageNames(std::vector<wxString>* names) const = 0;
	virtual IBuffer* GetPage(const wxString& pageName) = 0;
	static PageCollection* Create(const wxString& path);
protected:
	int m_pageCount;
};
