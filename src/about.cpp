/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2015-2025 Filip Szczerek <ga.software@yahoo.com>

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
#include <array>
#include <boost/version.hpp>
#if USE_OPENGL_BACKEND
#include <GL/glew.h>
#endif
#include <wx/bitmap.h>
#include <wx/button.h>
#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/filename.h>
#include <wx/gdicmn.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/timer.h>
#include <wx/utils.h>
#include <wx/wfstream.h>
#include <wx/window.h>
#if USE_CFITSIO
#include <fitsio.h>
#endif

#include "common/common.h"
#include "ctrl_ids.h"

#if (USE_FREEIMAGE)
#include <FreeImage.h> // must be the last #include, since it defined _WINDOWS_ and breaks WxW headers
#endif

const char* DATE_STR = "2025-11-18"; ///< Release date of the current version

#if !defined(_OPENMP)
int omp_get_num_procs() { return 1; }
#endif

constexpr size_t NUM_FRAMES = 30;
static struct
{
    bool valid = false;
    bool loaded = false;
    size_t frameIdx;
    wxSize frameSize;
    std::array<wxBitmap, NUM_FRAMES> frames;

    bool LoadFrames()
    {
        if (loaded) return true;

        static const size_t FRAME_NUM_BYTES[] = {
            1097,
            16948,
            31390,
            38289,
            52393,
            55880,
            58947,
            60470,
            66420,
            62632,
            66964,
            63927,
            64747,
            68776,
            60073,
            66680,
            61963,
            66462,
            62504,
            67139,
            64207,
            66606,
            62582,
            66302,
            65832,
            66281,
            60860,
            67799,
            61112,
            66956
        };

        // `images/anim.bin` contains concatenated frames, each is a PNG image

        wxFileName fName = GetImagesDirectory();
        fName.SetFullName("anim.bin");

        wxFileInputStream fstream(fName.GetFullPath());
        if (!fstream.IsOk())
            return false;

        wxFileOffset offset = 0;
        for (size_t i = 0; i < NUM_FRAMES; i++)
        {
            fstream.SeekI(offset);

            auto img = wxImage(fstream, "image/png");
            if (!img.IsOk())
                return false;
            if (i > 0 && img.GetSize() != frames[i-1].GetSize())
                return false;

            frames[i] = wxBitmap(img);
            if (i == 0)
                frameSize = frames[i].GetSize();

            offset += FRAME_NUM_BYTES[i];
        }

        loaded = true;
        return true;
    }
} Animation;

class c_AboutDialog: public wxDialog
{
    static constexpr unsigned ANIM_INTERVAL_MS = 40;
    static constexpr unsigned ANIM_REPLAY_DELAY_MS = 40;

    // Event handlers
    void OnPaintImgPanel(wxPaintEvent& event);
    void OnLibrariesClick(wxCommandEvent& event);
    void OnTimer(wxTimerEvent& event);

    wxTimer timer;

public:
    c_AboutDialog(wxWindow* parent);
};

void c_AboutDialog::OnLibrariesClick(wxCommandEvent&)
{
#if USE_OPENGL_BACKEND
    const wxString glRenderer{glGetString(GL_RENDERER)};
    const wxString glVersion{glGetString(GL_VERSION)};
#endif

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

#if USE_OPENGL_BACKEND
    if (!glRenderer.IsEmpty() && !glVersion.IsEmpty())
    {
        formatStr += "\n\nOpenGL: %s\n%s";
    }
#endif

    formatStr += "\n\nOS: " + wxGetOsDescription();

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
#if USE_OPENGL_BACKEND
        , wxString(glGetString(GL_VERSION))
        , wxString(glGetString(GL_RENDERER))
#endif
        ),
        _("Libraries"), wxOK, this);
}

void c_AboutDialog::OnTimer(wxTimerEvent&)
{
    this->Refresh(false);
    Animation.frameIdx = (Animation.frameIdx + 1) % NUM_FRAMES;
    if (Animation.frameIdx == NUM_FRAMES-1)
        timer.Start(ANIM_REPLAY_DELAY_MS, wxTIMER_ONE_SHOT);
    else
        timer.Start(ANIM_INTERVAL_MS, wxTIMER_ONE_SHOT);

}

c_AboutDialog::c_AboutDialog(wxWindow* parent)
{
    SetBackgroundColour(*wxBLACK);
    SetBackgroundStyle(wxBG_STYLE_PAINT);

    Create(parent, wxID_ANY, _("About ImPPG"));

    if (Animation.valid = Animation.LoadFrames())
    {
        Animation.frameIdx = 0;

        timer.SetOwner(this);
        Bind(wxEVT_TIMER, &c_AboutDialog::OnTimer, this);
        timer.Start(ANIM_INTERVAL_MS, wxTIMER_ONE_SHOT);

        SetMinClientSize(Animation.frameSize);
    }

    wxSizer* szTop = new wxBoxSizer(wxHORIZONTAL);
    szTop->AddStretchSpacer(1);

    wxSizer* szContents = new wxBoxSizer(wxVERTICAL);
    szContents->AddStretchSpacer(1);

    Bind(wxEVT_PAINT,
        [this](wxPaintEvent&)
        {
            wxAutoBufferedPaintDC dc(this);
            if (Animation.valid)
            {
                wxMemoryDC bmpDC(Animation.frames[Animation.frameIdx]);
                dc.Blit(wxPoint(0, 0), GetClientSize(), &bmpDC, wxPoint(0, 0));
            }
            else
            {
                dc.SetBrush(wxBrush(*wxBLACK));
                dc.DrawRectangle(wxRect(wxPoint(0, 0), GetClientSize()));
            }
        },
        wxID_ANY);

    wxStaticText* title = new wxStaticText(this, wxID_ANY, "ImPPG");
    title->SetFont(title->GetFont().MakeLarger().MakeBold());
    title->SetForegroundColour(*wxWHITE);
    szContents->Add(title, 0, wxALIGN_LEFT | (wxLEFT | wxRIGHT | wxTOP), 5);

    wxStaticText* info = new wxStaticText(this, wxID_ANY,
        wxString::Format(wxString(L"Copyright \u00A9 2015-2025 Filip Szczerek (ga.software@yahoo.com)\n") +
            _("version %d.%d.%d ") + " (%s)\n\n" +

            _("This program comes with ABSOLUTELY NO WARRANTY. This is free software,\n"
            "licensed under GNU General Public License v3 or any later version.\n"
            "See the LICENSE file for details."),
            IMPPG_VERSION_MAJOR, IMPPG_VERSION_MINOR, IMPPG_VERSION_SUBMINOR,
            DATE_STR
        )
    );
    info->SetForegroundColour(*wxWHITE);
    szContents->Add(info, 0, wxALIGN_LEFT | wxALL, 5);

    wxSizer* szSysInfo = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* cpuInfo = new wxStaticText(this, wxID_ANY, wxString::Format(_("Using %d logical CPU(s)."), omp_get_num_procs()));
    cpuInfo->SetForegroundColour(*wxWHITE);
    szSysInfo->Add(cpuInfo, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    wxButton* btnLibs = new wxButton(this, ID_Libraries, _("Libraries..."), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
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

    if (Animation.valid && sizerMinWidth > Animation.frameSize.GetWidth()/2)
    {
        for (wxBitmap& bmp: Animation.frames)
        {
            bmp = wxBitmap(bmp.ConvertToImage().Scale(2 * sizerMinWidth,
                                                      2 * sizerMinWidth * bmp.GetHeight() / bmp.GetWidth(),
                                                      wxIMAGE_QUALITY_BICUBIC));
        }
        Animation.frameSize = Animation.frames[0].GetSize();

        SetMinClientSize(Animation.frameSize);
    }

    Fit();

    /* We had to call SetMinClientSize() (as SetClientSize() is not sufficient under MS Windows) to make sure the dialog
       is as large as the bitmap. However, on GTK3 it makes the dialog resizable. Need to prevent this: */
    SetMaxClientSize(GetMinClientSize());

    // Center on the parent window
    if (parent)
        SetPosition(
          wxPoint(parent->GetPosition().x + (parent->GetSize().GetWidth() - this->GetSize().GetWidth())/2,
                  parent->GetPosition().y + (parent->GetSize().GetHeight() - this->GetSize().GetHeight())/2)
        );


    for (size_t i = 0; i < GetChildren().GetCount(); i++)
    {
        GetChildren()[i]->Bind(wxEVT_KEY_DOWN,
                               [this](wxKeyEvent& event)
                               {
                                    if (event.GetKeyCode() == WXK_ESCAPE)
                                        Close();
                               });

        if (GetChildren()[i]->GetId() != ID_Libraries)
            GetChildren()[i]->Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { Close(); });
    }

    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { Close(); });
}
void ShowAboutDialog(wxWindow* parent)
{
    c_AboutDialog aboutDlg(parent);
    aboutDlg.ShowModal();
}
