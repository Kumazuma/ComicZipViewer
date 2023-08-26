#pragma once
#define WX_PRECOMP
#include <wx/wxprec.h>
#include <wx/wx.h>
#undef GetHwnd
#include <wrl.h>
#include <vector>
#include <optional>
#include <sqlite3.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>
#include <unordered_map>
#include <wincodec.h>
using namespace Microsoft::WRL;