#pragma once
#include<wx/wx.h>

class View;
struct Model;
class ComicZipViewerApp: public wxApp
{
public:
    bool OnInit() override;
    int OnRun() override;
    Model& GetModel() {return *m_pModel; }
    bool OpenFile(const wxString& filePath);
private:
    View* m_pView;
    Model* m_pModel;
};

wxDECLARE_APP(ComicZipViewerApp);
