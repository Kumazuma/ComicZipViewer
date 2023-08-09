#pragma once
class NaturalSortOrder
{
public:
	bool operator() (const wxString& lhs, const wxString& rhs) const;
};

