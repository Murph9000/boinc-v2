// $Id$
//
// The contents of this file are subject to the BOINC Public License
// Version 1.0 (the "License"); you may not use this file except in
// compliance with the License. You may obtain a copy of the License at
// http://boinc.berkeley.edu/license_1.0.txt
// 
// Software distributed under the License is distributed on an "AS IS"
// basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
// License for the specific language governing rights and limitations
// under the License. 
// 
// The Original Code is the Berkeley Open Infrastructure for Network Computing. 
// 
// The Initial Developer of the Original Code is the SETI@home project.
// Portions created by the SETI@home project are Copyright (C) 2002
// University of California at Berkeley. All Rights Reserved. 
// 
// Contributor(s):
//
// Revision History:
//
// $Log$
// Revision 1.10  2004/05/21 06:27:15  rwalton
// *** empty log message ***
//
// Revision 1.9  2004/05/17 22:15:09  rwalton
// *** empty log message ***
//
//

#if defined(__GNUG__) && !defined(__APPLE__)
#pragma implementation "MainFrame.h"
#endif

#include "stdwx.h"
#include "BOINCGUIApp.h"
#include "MainFrame.h"
#include "BaseListCtrlView.h"
#include "BaseWindowView.h"
#include "MessagesView.h"
#include "ProjectsView.h"
#include "ResourceUtilizationView.h"
#include "TransfersView.h"
#include "WorkView.h"
#include "DlgAbout.h"
#include "DlgOptions.h"
#include "DlgAttachProject.h"

#include "res/BOINCGUIApp.xpm"


IMPLEMENT_DYNAMIC_CLASS(CMainFrame, wxFrame)

BEGIN_EVENT_TABLE (CMainFrame, wxFrame)
    EVT_CLOSE       (                           CMainFrame::OnClose)

    EVT_MENU        (wxID_EXIT,                 CMainFrame::OnExit)
    EVT_MENU        (ID_COMMANDSATTACHPROJECT,  CMainFrame::OnCommandsAttachProject)
    EVT_MENU        (ID_TOOLSOPTIONS,           CMainFrame::OnToolsOptions)
    EVT_MENU        (wxID_ABOUT,                CMainFrame::OnAbout)

    EVT_TIMER       (ID_FRAMERENDERTIMER,       CMainFrame::OnFrameRender)
END_EVENT_TABLE ()

CMainFrame::CMainFrame()
{
    wxLogTrace("CMainFrame::CMainFrame - Function Begining");

    wxLogTrace("CMainFrame::CMainFrame - Function Ending");
}


CMainFrame::CMainFrame(wxString strTitle) : 
    wxFrame ((wxFrame *)NULL, -1, strTitle, wxDefaultPosition, wxDefaultSize,
             wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE)
{
    wxLogTrace("CMainFrame::CMainFrame - Function Begining");


    m_pMenubar = NULL;
    m_pNotebook = NULL;
    m_pStatusbar = NULL;


    SetIcon(wxICON(APP_ICON));


    wxCHECK_RET(CreateMenu(), _T("Failed to create menu bar."));
    wxCHECK_RET(CreateNotebook(), _T("Failed to create notebook."));
    wxCHECK_RET(CreateStatusbar(), _T("Failed to create status bar."));


    m_pFrameRenderTimer = new wxTimer(this, ID_FRAMERENDERTIMER);
    wxASSERT(NULL != m_pFrameRenderTimer);

    m_pFrameRenderTimer->Start(1000);       // Send event every second


    wxLogTrace("CMainFrame::CMainFrame - Function Ending");
}


CMainFrame::~CMainFrame(){
    wxLogTrace("CMainFrame::~CMainFrame - Function Begining");


    wxASSERT(NULL != m_pFrameRenderTimer);
    wxASSERT(NULL != m_pMenubar);
    wxASSERT(NULL != m_pNotebook);
    wxASSERT(NULL != m_pStatusbar);


    if (m_pFrameRenderTimer) {
        m_pFrameRenderTimer->Stop();
        delete m_pFrameRenderTimer;
    }

    if (m_pStatusbar)
        wxCHECK_RET(DeleteStatusbar(), _T("Failed to delete status bar."));

    if (m_pNotebook)
        wxCHECK_RET(DeleteNotebook(), _T("Failed to delete notebook."));

    if (m_pMenubar)
        wxCHECK_RET(DeleteMenu(), _T("Failed to delete menu bar."));


    wxLogTrace("CMainFrame::~CMainFrame - Function Ending");
}


bool CMainFrame::CreateMenu() {
    wxLogTrace("CMainFrame::CreateMenu - Function Begining");


    // File menu
    wxMenu *menuFile = new wxMenu;
    menuFile->Append(wxID_EXIT, _("E&xit"));

    // Commands menu
    wxMenu *menuCommands = new wxMenu;
    menuCommands->Append(ID_COMMANDSATTACHPROJECT, _("&Attach to Project..."));

    // Tools menu
    wxMenu *menuTools = new wxMenu;
    menuTools->Append(ID_TOOLSOPTIONS, _("&Options"));

    // Help menu
    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT, _("&About BOINC..."));

    // construct menu
    m_pMenubar = new wxMenuBar;
    m_pMenubar->Append(menuFile,      _("&File"));
    m_pMenubar->Append(menuCommands,  _("&Commands"));
    m_pMenubar->Append(menuTools,     _("&Tools"));
    m_pMenubar->Append(menuHelp,      _("&Help"));
    SetMenuBar(m_pMenubar);


    wxLogTrace("CMainFrame::CreateMenu - Function Ending");
    return true;
}


bool CMainFrame::CreateNotebook() {
    wxLogTrace("CMainFrame::CreateNotebook - Function Begining");


    // create frame panel
    wxPanel *pPanel = new wxPanel(this, -1, wxDefaultPosition, wxDefaultSize,
                                 wxTAB_TRAVERSAL|wxCLIP_CHILDREN|wxNO_BORDER);

    // initialize notebook
    m_pNotebook = new wxNotebook(pPanel, -1, wxDefaultPosition, wxDefaultSize,
                                wxNB_FIXEDWIDTH|wxCLIP_CHILDREN);

    wxNotebookSizer *pNotebookSizer = new wxNotebookSizer(m_pNotebook);

    // layout frame panel
    wxBoxSizer *pPanelSizer = new wxBoxSizer(wxVERTICAL);
    pPanelSizer->Add(new wxStaticLine(pPanel, -1), 0, wxEXPAND);
    pPanelSizer->Add(0, 4);
    pPanelSizer->Add(pNotebookSizer, 1, wxEXPAND);
    pPanel->SetAutoLayout(true);
    pPanel->SetSizerAndFit(pPanelSizer);

    CreateNotebookPage(new CProjectsView(m_pNotebook));
    CreateNotebookPage(new CWorkView(m_pNotebook));
    CreateNotebookPage(new CTransfersView(m_pNotebook));
    CreateNotebookPage(new CMessagesView(m_pNotebook));
    CreateNotebookPage(new CResourceUtilizationView(m_pNotebook));


    wxLogTrace("CMainFrame::CreateNotebook - Function Ending");
    return true;
}


bool CMainFrame::CreateNotebookPage(wxWindow* pwndNewNotebookPage) {
    wxLogTrace("CMainFrame::CreateNotebookPage - Function Begining");


    wxImageList*    pImageList;
    int             iImageIndex = 0;

    wxASSERT(NULL != pwndNewNotebookPage);
    wxASSERT(NULL != m_pNotebook);
    wxASSERT(wxDynamicCast(pwndNewNotebookPage, CBaseListCtrlView) ||
             wxDynamicCast(pwndNewNotebookPage, CBaseWindowView));


    pImageList = m_pNotebook->GetImageList();
    if (!pImageList) {
        pImageList = new wxImageList(16, 16, true, 0);
        wxASSERT(pImageList != NULL);
        m_pNotebook->SetImageList(pImageList);
    }
    
    if        (wxDynamicCast(pwndNewNotebookPage, CProjectsView)) {

        CProjectsView* pPage = wxDynamicCast(pwndNewNotebookPage, CProjectsView);
        iImageIndex = pImageList->Add(wxBitmap(pPage->GetViewIcon()), wxColour(255, 0, 255));
        m_pNotebook->AddPage(pPage, pPage->GetViewName(), TRUE, iImageIndex);

    } else if (wxDynamicCast(pwndNewNotebookPage, CWorkView)) {

        CWorkView* pPage = wxDynamicCast(pwndNewNotebookPage, CWorkView);
        iImageIndex = pImageList->Add(wxBitmap(pPage->GetViewIcon()), wxColour(255, 0, 255));
        m_pNotebook->AddPage(pPage, pPage->GetViewName(), TRUE, iImageIndex);

    } else if (wxDynamicCast(pwndNewNotebookPage, CTransfersView)) {

        CTransfersView* pPage = wxDynamicCast(pwndNewNotebookPage, CTransfersView);
        iImageIndex = pImageList->Add(wxBitmap(pPage->GetViewIcon()), wxColour(255, 0, 255));
        m_pNotebook->AddPage(pPage, pPage->GetViewName(), TRUE, iImageIndex);

    } else if (wxDynamicCast(pwndNewNotebookPage, CMessagesView)) {

        CMessagesView* pPage = wxDynamicCast(pwndNewNotebookPage, CMessagesView);
        iImageIndex = pImageList->Add(wxBitmap(pPage->GetViewIcon()), wxColour(255, 0, 255));
        m_pNotebook->AddPage(pPage, pPage->GetViewName(), TRUE, iImageIndex);

    } else if (wxDynamicCast(pwndNewNotebookPage, CResourceUtilizationView)) {

        CResourceUtilizationView* pPage = wxDynamicCast(pwndNewNotebookPage, CResourceUtilizationView);
        iImageIndex = pImageList->Add(wxBitmap(pPage->GetViewIcon()), wxColour(255, 0, 255));
        m_pNotebook->AddPage(pPage, pPage->GetViewName(), TRUE, iImageIndex);

    } else if (wxDynamicCast(pwndNewNotebookPage, CBaseListCtrlView)) {

        CBaseListCtrlView* pPage = wxDynamicCast(pwndNewNotebookPage, CBaseListCtrlView);
        iImageIndex = pImageList->Add(wxBitmap(pPage->GetViewIcon()), wxColour(255, 0, 255));
        m_pNotebook->AddPage(pPage, pPage->GetViewName(), TRUE, iImageIndex);

    } else if (wxDynamicCast(pwndNewNotebookPage, CBaseWindowView)) {

        CBaseWindowView* pPage = wxDynamicCast(pwndNewNotebookPage, CBaseWindowView);
        iImageIndex = pImageList->Add(wxBitmap(pPage->GetViewIcon()), wxColour(255, 0, 255));
        m_pNotebook->AddPage(pPage, pPage->GetViewName(), TRUE, iImageIndex);

    }


    wxLogTrace("CMainFrame::CreateNotebookPage - Function Ending");
    return true;
}


bool CMainFrame::CreateStatusbar() {
    wxLogTrace("CMainFrame::CreateStatusbar - Function Begining");


    if (m_pStatusbar)
        return true;

    int ch = GetCharWidth();

    const int widths[] = {-1, 20*ch, 15};

    m_pStatusbar = CreateStatusBar(WXSIZEOF(widths), wxST_SIZEGRIP, ID_STATUSBAR);
    wxASSERT(NULL != m_pStatusbar);

    m_pStatusbar->SetStatusWidths(WXSIZEOF(widths), widths);

    SetStatusBar(m_pStatusbar);
    SendSizeEvent();


    wxLogTrace("CMainFrame::CreateStatusbar - Function Ending");
    return true;
}


bool CMainFrame::DeleteMenu() {
    wxLogTrace("CMainFrame::DeleteMenu - Function Begining");

    wxLogTrace("CMainFrame::DeleteMenu - Function Ending");
    return true;
}


bool CMainFrame::DeleteNotebook() {
    wxLogTrace("CMainFrame::DeleteNotebook - Function Begining");


    wxImageList*    pImageList;

    wxASSERT(NULL != m_pNotebook);

    pImageList = m_pNotebook->GetImageList();

    wxASSERT(NULL != pImageList);

    if (pImageList)
        delete pImageList;


    wxLogTrace("CMainFrame::DeleteNotebook - Function Ending");
    return true;
}


bool CMainFrame::DeleteStatusbar() {
    wxLogTrace("CMainFrame::DeleteStatusbar - Function Begining");


    if (!m_pStatusbar)
        return true;

    SetStatusBar(NULL);

    delete m_pStatusbar;

    m_pStatusbar = NULL;
    SendSizeEvent();


    wxLogTrace("CMainFrame::DeleteStatusbar - Function Ending");
    return true;
}


bool CMainFrame::SaveState() {
    wxLogTrace("CMainFrame::SaveState - Function Begining");

    wxLogTrace("CMainFrame::DeleteStatusbar - Function Ending");
    return true;
}


bool CMainFrame::RestoreState() {
    wxLogTrace("CMainFrame::RestoreState - Function Begining");

    wxLogTrace("CMainFrame::RestoreState - Function Ending");
    return true;
}


void CMainFrame::OnExit(wxCommandEvent &WXUNUSED(event)) {
    wxLogTrace("CMainFrame::OnExit - Function Begining");

    Close(true);

    wxLogTrace("CMainFrame::OnExit - Function Ending");
}


void CMainFrame::OnClose(wxCloseEvent &event) {
    wxLogTrace("CMainFrame::OnClose - Function Begining");

    Destroy();

    wxLogTrace("CMainFrame::OnClose - Function Ending");
}


void CMainFrame::OnCommandsAttachProject(wxCommandEvent &WXUNUSED(event)) {
    wxLogTrace("CMainFrame::OnCommandsAttachProject - Function Begining");


    CDlgAttachProject* pDlg = new CDlgAttachProject(this);
    wxASSERT(NULL != pDlg);

    pDlg->ShowModal();

    if (pDlg)
        delete pDlg;


    wxLogTrace("CMainFrame::OnCommandsAttachProject - Function Ending");
}


void CMainFrame::OnToolsOptions(wxCommandEvent &WXUNUSED(event)) {
    wxLogTrace("CMainFrame::OnToolsOptions - Function Begining");


    CDlgOptions* pDlg = new CDlgOptions(this);
    wxASSERT(NULL != pDlg);

    pDlg->ShowModal();

    if (pDlg)
        delete pDlg;


    wxLogTrace("CMainFrame::OnToolsOptions - Function Ending");
}


void CMainFrame::OnAbout(wxCommandEvent &WXUNUSED(event)) {
    wxLogTrace("CMainFrame::OnAbout - Function Begining");


    CDlgAbout* pDlg = new CDlgAbout(this);
    wxASSERT(NULL != pDlg);

    pDlg->ShowModal();

    if (pDlg)
        delete pDlg;


    wxLogTrace("CMainFrame::OnAbout - Function Ending");
}


void CMainFrame::OnFrameRender (wxTimerEvent &event) {
    wxLogTrace("CMainFrame::OnFrameRender - Function Begining");

    wxWindow*       pwndCurrentlySelectedPage;

    wxASSERT(NULL != m_pNotebook);


    pwndCurrentlySelectedPage = m_pNotebook->GetPage(m_pNotebook->GetSelection());
    wxASSERT(NULL != pwndCurrentlySelectedPage);
    wxASSERT(wxDynamicCast(pwndCurrentlySelectedPage, CBaseListCtrlView) ||
             wxDynamicCast(pwndCurrentlySelectedPage, CBaseWindowView));


    if        (wxDynamicCast(pwndCurrentlySelectedPage, CProjectsView)) {

        CProjectsView* pPage = wxDynamicCast(pwndCurrentlySelectedPage, CProjectsView);
        pPage->OnRender(event);

    } else if (wxDynamicCast(pwndCurrentlySelectedPage, CWorkView)) {

        CWorkView* pPage = wxDynamicCast(pwndCurrentlySelectedPage, CWorkView);
        pPage->OnRender(event);

    } else if (wxDynamicCast(pwndCurrentlySelectedPage, CTransfersView)) {

        CTransfersView* pPage = wxDynamicCast(pwndCurrentlySelectedPage, CTransfersView);
        pPage->OnRender(event);

    } else if (wxDynamicCast(pwndCurrentlySelectedPage, CMessagesView)) {

        CMessagesView* pPage = wxDynamicCast(pwndCurrentlySelectedPage, CMessagesView);
        pPage->OnRender(event);

    } else if (wxDynamicCast(pwndCurrentlySelectedPage, CResourceUtilizationView)) {

        CResourceUtilizationView* pPage = wxDynamicCast(pwndCurrentlySelectedPage, CResourceUtilizationView);
        pPage->OnRender(event);

    } else if (wxDynamicCast(pwndCurrentlySelectedPage, CBaseListCtrlView)) {

        CBaseListCtrlView* pPage = wxDynamicCast(pwndCurrentlySelectedPage, CBaseListCtrlView);
        pPage->OnRender(event);

    } else if (wxDynamicCast(pwndCurrentlySelectedPage, CBaseWindowView)) {

        CBaseWindowView* pPage = wxDynamicCast(pwndCurrentlySelectedPage, CBaseWindowView);
        pPage->OnRender(event);

    }


    wxLogTrace("CMainFrame::OnFrameRender - Function Ending");
}

