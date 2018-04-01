/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2015-2017 Filip Szczerek <ga.software@yahoo.com>

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
#include <wx/stdpaths.h>
#include <wx/utils.h>
#if (USE_FREEIMAGE)
  #ifndef _WINDOWS_
    #define FIMG_CLEANUP
  #endif
  #include <FreeImage.h>
  #ifdef FIMG_CLEANUP
    // FreeImage.h always defines _WINDOWS_, which interferes with wx headers
    #undef _WINDOWS_
    #undef FIMG_CLEANUP
  #endif
#endif // USE_FREEIMAGE
#if USE_CFITSIO
#include <fitsio.h>
#endif
#include "common.h"
#include "ctrl_ids.h"

const char *VERSION_STR = "0.5.3";   ///< Current version
const char *DATE_STR = "2017-03-12"; ///< Release date of the current version

#if !defined(_OPENMP)
int omp_get_num_procs() { return 1; }
#endif

class c_AboutDialog: public wxDialog
{
    void OnPaintImgPanel(wxPaintEvent &event);
    void OnLibrariesClick(wxCommandEvent &event);

    wxBitmap m_Bmp;

public:
    c_AboutDialog(wxWindow *parent);
};

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
                        "\n\n" + wxGetOsDescription()
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

c_AboutDialog::c_AboutDialog(wxWindow *parent)
: wxDialog(parent, wxID_ANY, _("About ImPPG"))
{
    SetBackgroundColour(*wxBLACK);

    m_Bmp = LoadBitmap("about");
    SetMinClientSize(m_Bmp.GetSize());

    wxSizer *szTop = new wxBoxSizer(wxHORIZONTAL);
    szTop->AddStretchSpacer(1);

    wxSizer *szContents = new wxBoxSizer(wxVERTICAL);
    szContents->AddStretchSpacer(1);

    Bind(wxEVT_PAINT,
         [this](wxPaintEvent &evt)
         {
            wxPaintDC dc(this);
            wxMemoryDC bmpDC(m_Bmp);
            dc.Blit(wxPoint(0, 0), GetClientSize(), &bmpDC, wxPoint(0, 0));
         },
         wxID_ANY);


    wxStaticText *title = new wxStaticText(this, wxID_ANY, "ImPPG");
    title->SetFont(title->GetFont().MakeLarger().MakeBold());
    title->SetForegroundColour(*wxWHITE);
    szContents->Add(title, 0, wxALIGN_LEFT | (wxLEFT | wxRIGHT | wxTOP), 5);

    wxStaticText *info = new wxStaticText(this, wxID_ANY,
        wxString::Format(wxString(L"Copyright \u00A9 2015-2017 Filip Szczerek (ga.software@yahoo.com)\n") +
                         _("version %s ") + " (%s)\n\n" +

                         _("This program comes with ABSOLUTELY NO WARRANTY. This is free software,\n"
                           "licensed under GNU General Public License v3 or any later version.\n"
                           "See the LICENSE file for details."), VERSION_STR, DATE_STR));
    info->SetForegroundColour(*wxWHITE);
    szContents->Add(info, 0, wxALIGN_LEFT | wxALL, 5);

    wxSizer *szSysInfo = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText *cpuInfo = new wxStaticText(this, wxID_ANY, wxString::Format(_("Using %d logical CPU(s)."), omp_get_num_procs()));
    cpuInfo->SetForegroundColour(*wxWHITE);
    szSysInfo->Add(cpuInfo, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    wxButton *btnLibs = new wxButton(this, ID_Libraries, _("Libraries..."), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    btnLibs->SetForegroundColour(*wxWHITE);
    btnLibs->SetBackgroundColour(*wxBLACK);
    btnLibs->Bind(wxEVT_BUTTON, &c_AboutDialog::OnLibrariesClick, this);
    szSysInfo->Add(btnLibs, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    szContents->Add(szSysInfo, 0, wxALIGN_LEFT, 0);

    szContents->AddStretchSpacer(3);

    szTop->Add(szContents, 0, wxALIGN_TOP | wxGROW);
    szTop->AddStretchSpacer(3);

    SetSizer(szTop);
    Fit();

    int sizerMinWidth = GetSizer()->GetMinSize().GetWidth(); // min. width based on combined children's sizes

    if (sizerMinWidth > m_Bmp.GetWidth()/2)
    {
        m_Bmp = wxBitmap(m_Bmp.ConvertToImage().Scale(2*sizerMinWidth,
                                                      2*sizerMinWidth*m_Bmp.GetHeight() / m_Bmp.GetWidth(),
                                                      wxIMAGE_QUALITY_BICUBIC));

        SetMinClientSize(m_Bmp.GetSize());
    }

    Fit();

    /* We had to call SetMinClientSize() (as SetClientSize() is not sufficient under MS Windows) to make sure the dialog
       is as large as the bitmap. However, on GTK3 it makes the dialog resizable. Need to prevent this: */
    SetMaxClientSize(GetMinClientSize());

    // Center on the parent window
    if (parent)
        SetPosition(
          wxPoint(parent->GetPosition().x + (parent->GetSize().GetWidth() - this->GetSize().GetWidth())/2,
                  parent->GetPosition().y + (parent->GetSize().GetHeight() - this->GetSize().GetHeight())/2));


    for (size_t i = 0; i < GetChildren().GetCount(); i++)
    {
        GetChildren()[i]->Bind(wxEVT_KEY_DOWN,
                               [this](wxKeyEvent &event)
                               {
                                    if (event.GetKeyCode() == WXK_ESCAPE)
                                        Close();
                               });

        if (GetChildren()[i]->GetId() != ID_Libraries)
            GetChildren()[i]->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &event) { Close(); });
    }

    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &event) { Close(); });
}
void ShowAboutDialog(wxWindow *parent)
{
    c_AboutDialog aboutDlg(parent);
    aboutDlg.ShowModal();
}
