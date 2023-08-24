#pragma once
#include "framework.h"
#include <wx/wx.h>
#include <unordered_set>

struct Model
{
	wxString openedPath;
	std::vector<wxString> pageList;
	std::unordered_set<wxString> markedPageSet;
	int currentPageNumber;
	wxString pageName;
	std::vector<wxString> bookList;
};
