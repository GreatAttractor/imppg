/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2015, 2016 Filip Szczerek <ga.software@yahoo.com>

This file is part of ImPPG.

ImPPG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

ImPPG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ImPPG.  If not, see <http://www.gnu.org/licenses/>.

File description:
    "About" dialog implementation.
*/

#if defined(_OPENMP)
#include <omp.h>
#endif
#include <boost/version.hpp>
#include <wx/window.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/event.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/bitmap.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>
#include <wx/gdicmn.h>
#include <wx/panel.h>
#if (USE_FREEIMAGE)
#include <FreeImage.h>
#endif
#if USE_CFITSIO
#include <cfitsio/fitsio.h>
#endif
#include "ctrl_ids.h"

const char *VERSION_STR = "0.5";   ///< Current version
const char *DATE_STR = "2016-01-02"; ///< Release date of the current version

#if !defined(_OPENMP)
int omp_get_num_procs() { return 1; }
#endif

class c_AboutDialog: public wxDialog
{
    void OnPaintImgPanel(wxPaintEvent &event);
    void OnLeftDown(wxMouseEvent &event);
    void OnKeyDown(wxKeyEvent &event);
    void OnLibrariesClick(wxCommandEvent &event);

    wxPanel *m_ImgPanel;
    wxBitmap m_Bmp;

public:
    c_AboutDialog(wxWindow *parent);

    DECLARE_EVENT_TABLE();
};

BEGIN_EVENT_TABLE(c_AboutDialog, wxDialog)
    EVT_LEFT_DOWN(c_AboutDialog::OnLeftDown)
    EVT_KEY_DOWN(c_AboutDialog::OnKeyDown)
    EVT_BUTTON(ID_Libraries, c_AboutDialog::OnLibrariesClick)
END_EVENT_TABLE()

//-----------------------------------------------------------

void c_AboutDialog::OnLibrariesClick(wxCommandEvent &event)
{
    wxString formatStr = "%s\n\n"    // "Libraries" - a localized string
                         "%s\n"      // version of wxWidgets
                         "Boost %s"  // version of Boost
#if USE_FREEIMAGE
                         "\nFreeImage %s" // version of FreeImage
#endif

#if USE_CFITSIO
                         "\nCFITSIO %s" // version of CFITSIO
#endif
    ;

#if USE_CFITSIO
    float cfitsioVer;
    fits_get_version(&cfitsioVer);
#endif

    wxMessageBox(wxString::Format(formatStr, _("Library versions:"),
        wxVERSION_STRING,
        wxString::Format("%d.%d.%d", BOOST_VERSION / 100000, (BOOST_VERSION / 100) % 1000, (BOOST_VERSION % 100))
#if USE_FREEIMAGE
        , FreeImage_GetVersion()
#endif
#if USE_CFITSIO
        , wxString::FromCDouble(cfitsioVer, 2)
#endif
        ),
        _("Libraries"), wxOK, this);
}

void c_AboutDialog::OnKeyDown(wxKeyEvent &event)
{
    if (event.GetKeyCode() == WXK_ESCAPE)
        Close();
}

void c_AboutDialog::OnLeftDown(wxMouseEvent &event)
{
    Close();
}

void c_AboutDialog::OnPaintImgPanel(wxPaintEvent &event)
{
    wxPaintDC dc(m_ImgPanel);
    wxMemoryDC bmpDC(m_Bmp);
    dc.Blit(wxPoint(0, 0), GetClientSize(), &bmpDC, wxPoint(0, 0));
}

c_AboutDialog::c_AboutDialog(wxWindow *parent)
: wxDialog(parent, wxID_ANY, _("About ImPPG"))
{
    SetBackgroundColour(*wxWHITE);

    wxFileName logoFileName("images", "about", "png");
    wxImage img(logoFileName.GetFullPath());
    if (!img.IsOk())
    {
        wxMessageBox(wxString::Format(_("Cannot load bitmap: %s"), logoFileName.GetFullName()), _("Error"), wxICON_ERROR);
        m_Bmp = wxBitmap(1, 1);
    }
    else
    {
        m_Bmp = wxBitmap(img);
    }

    wxSizer *szTop = new wxBoxSizer(wxVERTICAL);

    m_ImgPanel = new wxPanel(this);
    m_ImgPanel->SetMinClientSize(m_Bmp.GetSize());
    m_ImgPanel->Bind(wxEVT_PAINT, &c_AboutDialog::OnPaintImgPanel, this);
    szTop->Add(m_ImgPanel, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 0);

    wxStaticText *title = new wxStaticText(this, wxID_ANY, "ImPPG");
    title->SetFont(title->GetFont().MakeLarger().MakeBold());
    szTop->Add(title, 0, wxALIGN_LEFT | (wxLEFT | wxRIGHT | wxTOP), 5);

    szTop->Add(new wxStaticText(this, wxID_ANY,
        wxString::Format(wxString(L"Copyright \u00A9 2015, 2016 Filip Szczerek (ga.software@yahoo.com)\n") +
                         _("version %s ") + " (%s)\n\n" +

                         _("This program comes with ABSOLUTELY NO WARRANTY. This is free software,\n"
                           "licensed under GNU General Public License v3 or any later version.\n"
                           "See the LICENSE file for details."), VERSION_STR, DATE_STR)),
        0, wxALIGN_LEFT | wxALL, 5);


    wxSizer *szSysInfo = new wxBoxSizer(wxHORIZONTAL);
    szSysInfo->Add(new wxStaticText(this, wxID_ANY, wxString::Format(_("Using %d logical CPU(s)."), omp_get_num_procs())), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    szSysInfo->Add(new wxButton(this, ID_Libraries, _("Libraries..."), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    szTop->Add(szSysInfo, 0, wxALIGN_LEFT, 0);

    SetSizer(szTop);
    Fit();
    // Center on the parent window
    if (parent)
        SetPosition(
          wxPoint(parent->GetPosition().x + (parent->GetSize().GetWidth() - this->GetSize().GetWidth())/2,
                  parent->GetPosition().y + (parent->GetSize().GetHeight() - this->GetSize().GetHeight())/2));


    for (size_t i = 0; i < GetChildren().GetCount(); i++)
    {
        GetChildren()[i]->Bind(wxEVT_KEY_DOWN, &c_AboutDialog::OnKeyDown, this);
        if (GetChildren()[i]->GetId() != ID_Libraries)
            GetChildren()[i]->Bind(wxEVT_LEFT_DOWN, &c_AboutDialog::OnLeftDown, this);
    }
}
void ShowAboutDialog(wxWindow *parent)
{
    c_AboutDialog aboutDlg(parent);
    aboutDlg.ShowModal();
}
