#pragma once
#include "framework.h"
#include <wx/wx.h>
struct Model
{
	wxString openedPath;
	std::vector<wxString> pageList;
	int currentPageNumber;
	wxString pageName;
};
