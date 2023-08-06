#include "framework.h"
#include "ComicZipViewer.h"
#include "View.h"
#include "Model.h"

wxIMPLEMENT_APP(ComicZipViewerApp);

bool ComicZipViewerApp::OnInit()
{
    m_pModel = new Model();
    m_pView = new View();
    m_pView->Show();
    return true;
}

int ComicZipViewerApp::OnRun()
{
    int ret = wxApp::OnRun();
    delete m_pView;
    delete m_pModel;
    return ret;
}

bool ComicZipViewerApp::OpenFile(const wxString& filePath)
{

    return false;
}
