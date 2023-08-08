#pragma once
#include <wx/wx.h>
#include "PageCollection.h"

class View;
struct Model;
class ComicZipViewerApp: public wxApp
{
public:
	ComicZipViewerApp();
	bool OnInit() override;
	int OnRun() override;
	Model& GetModel() {return *m_pModel; }
	bool OpenFile(const wxString& filePath);
	uint32_t GetPageCount() const;
	bool GetPageName(uint32_t idx, wxString* filename);
	wxImage GetDecodedImage(uint32_t idx);
	const wxString& GetCurrentPageName() const;
	int GetCurrentPageNumber() const;
private:
	View* m_pView;
	Model* m_pModel;
	PageCollection* m_pPageCollection;
	wxPNGHandler m_pngHandler;
	wxGIFHandler m_gifHandler;
	wxJPEGHandler m_jpegHandler;
};

wxDECLARE_APP(ComicZipViewerApp);
