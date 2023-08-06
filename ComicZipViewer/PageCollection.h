#pragma once
class PageCollection
{
public:
    virtual ~PageCollection() = default;
    int GetPageCount() const { return m_pageCount; }

protected:
    int m_pageCount;
};
