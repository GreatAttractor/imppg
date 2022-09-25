/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2019 Filip Szczerek <ga.software@yahoo.com>

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
    Image alignment parameters dialog.
*/

#include <wx/artprov.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/editlbox.h>
#include <wx/event.h>
#include <wx/filename.h>
#include <wx/filepicker.h>
#include <wx/generic/statbmpg.h>
#include <wx/msgdlg.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/valgen.h>
#include <wx/window.h>

#include "align.h"
#include "appconfig.h"
#include "common/common.h"
#include "common/formats.h"
#include "scrollable_dlg.h"

enum
{
    ID_AddFiles = wxID_HIGHEST,
    ID_Start,
    ID_RemoveSelected,
    ID_FileList,
    ID_OutputDir,
    ID_SubpixelAlignment,
    ID_Sort,
    ID_RemoveAll,
    ID_Crop,
    ID_CropBitmap,
    ID_Method,
    ID_MethodBitmap
};

const int BORDER = 5; ///< Border size (in pixels) between controls

wxString GetAlignmentMethodDescription(AlignmentMethod method)
{
    switch (method)
    {
    case AlignmentMethod::PHASE_CORRELATION: return
        _("A general-purpose method. Keeps the high-contrast features\n"
          "(e.g. sunspots, filaments, prominences, craters) stationary.");

    case AlignmentMethod::LIMB: return
        _(L"Keeps the solar limb stationary. The more of the limb is visible\n"
          L"in input images, the better the results. Requirements:\n"
          L"\u2013 the disc has to be brighter than the background\n"
          L"\u2013 no vignetting or limb darkening exaggerated by post-processing\n"
          L"\u2013 the disc must not be eclipsed by the Moon");

    default: return wxEmptyString;
    }
}

class c_ImageAlignmentParams: public c_ScrollableDialog
{
    void OnCommandEvent(wxCommandEvent& event);
    void OnOutputDirChanged(wxFileDirPickerEvent& event);
    void OnContentsScrolled(wxScrollWinEvent& event);
    void OnContentsResized(wxSizeEvent& event);

    void DoInitControls() override;

    AlignmentParameters_t m_Parameters;

    wxDirPickerCtrl* m_OutputDirCtrl{nullptr};
    wxEditableListBox m_FileList;
    wxGenericStaticBitmap* m_CropBitmapCtrl{nullptr};
    wxStaticText* m_AlignMethodTextCtrl{nullptr};

    wxBitmap m_CropBitmaps[2]; ///< Bitmaps illustrating the "pad to bounding box" and "crop to intersection" output modes

public:
    c_ImageAlignmentParams(wxWindow* parent);

    void GetAlignmentParameters(AlignmentParameters_t& params);

    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(c_ImageAlignmentParams, wxDialog)
    EVT_BUTTON(ID_AddFiles, c_ImageAlignmentParams::OnCommandEvent)
    EVT_BUTTON(ID_Sort, c_ImageAlignmentParams::OnCommandEvent)
    EVT_BUTTON(ID_RemoveAll, c_ImageAlignmentParams::OnCommandEvent)
    EVT_BUTTON(ID_Start, c_ImageAlignmentParams::OnCommandEvent)
    EVT_RADIOBOX(ID_Crop, c_ImageAlignmentParams::OnCommandEvent)
    EVT_RADIOBOX(ID_Method, c_ImageAlignmentParams::OnCommandEvent)
    EVT_DIRPICKER_CHANGED(ID_OutputDir, c_ImageAlignmentParams::OnOutputDirChanged)
END_EVENT_TABLE()

//--------------------------------------------------------------------------------------------

void c_ImageAlignmentParams::GetAlignmentParameters(AlignmentParameters_t& params)
{
    params = m_Parameters;
    params.inputFiles.Clear();
    m_FileList.GetStrings(params.inputFiles);
    params.outputDir = m_Parameters.outputDir = m_OutputDirCtrl->GetPath();
}

void c_ImageAlignmentParams::OnOutputDirChanged(wxFileDirPickerEvent& event)
{
#ifdef __WXMSW__
    // There is a bug in Windows in the "FilePicker in Folder Select mode" common dialog,
    // where the deepest selected sub-folder may appear twice at the end of the returned path.
    // Detect and fix it.
    // However, we may get this event also when the user is editing the path using the text field,
    // so that the path may be temporarily incorrect. We assume that's the case if the path with
    // the last dir removed also doesn't exist - we make no change then.

    wxFileName path(event.GetPath());

    if (!path.Exists() && path.GetDirCount() > 0)
    {
        path.RemoveLastDir();
        if (path.Exists())
            m_OutputDirCtrl->SetPath(path.GetFullPath());
        // else: do nothing, apparently the user is editing the path manually, so it's temporarily incorrect
    }
#else
    (void)event;
#endif
}

void c_ImageAlignmentParams::OnCommandEvent(wxCommandEvent& event)
{
    switch (event.GetId())
    {
    case ID_Crop:
        m_CropBitmapCtrl->SetBitmap(m_CropBitmaps[event.GetInt()]);
        break;

    case ID_Method:
        m_AlignMethodTextCtrl->SetLabel(GetAlignmentMethodDescription(static_cast<AlignmentMethod>(event.GetInt())));
        Layout();
        break;

    case ID_AddFiles:
        {
            wxFileDialog fileDlg(this, _("Choose input file(s)"), Configuration::AlignInputPath, wxEmptyString, INPUT_FILE_FILTERS, wxFD_OPEN | wxFD_MULTIPLE);
            if (fileDlg.ShowModal() == wxID_OK)
            {
                Configuration::AlignInputPath = wxFileName(fileDlg.GetPath()).GetPath();
                wxArrayString files;
                m_FileList.GetStrings(files);

                wxArrayString newFiles;
                fileDlg.GetPaths(newFiles);
                for (size_t i = 0; i < newFiles.Count(); i++)
                    files.Add(newFiles[i]);

                m_FileList.SetStrings(files);
            }
            break;
        }

    case ID_Sort:
        {
            wxArrayString files;
            m_FileList.GetStrings(files);
            files.Sort();
            m_FileList.SetStrings(files);
            break;
        }

    case ID_RemoveAll:
        m_FileList.SetStrings(wxArrayString());
        break;

    case ID_Start:
        {
            wxArrayString strings;
            m_FileList.GetStrings(strings);
            if (strings.Count() == 0)
                wxMessageBox(_("No input files selected."), _("Error"), wxICON_ERROR, this);
            else if (!wxFileName::DirExists(m_OutputDirCtrl->GetPath()))
                wxMessageBox(_("Selected output folder does not exist."), _("Error"), wxICON_ERROR, this);
            else
            {
                TransferDataFromWindow();
                EndModal(wxID_OK);
            }
            break;
        }

    }
}

c_ImageAlignmentParams::c_ImageAlignmentParams(wxWindow* parent)
    : c_ScrollableDialog(parent, wxID_ANY, _("Image alignment"))
{
    InitControls(BORDER);
}

void c_ImageAlignmentParams::DoInitControls()
{
    m_Parameters.cropMode = CropMode::CROP_TO_INTERSECTION;
    m_Parameters.subpixelAlignment = true;
    m_Parameters.alignmentMethod = AlignmentMethod::PHASE_CORRELATION;
    m_Parameters.normalizeFitsValues = Configuration::NormalizeFITSValues;

    m_CropBitmaps[static_cast<size_t>(CropMode::CROP_TO_INTERSECTION)] = LoadBitmap("crop");
    m_CropBitmaps[static_cast<size_t>(CropMode::PAD_TO_BOUNDING_BOX)] = LoadBitmap("pad");

    wxSizer* szContents = new wxBoxSizer(wxVERTICAL);

        wxSizer* szListButtons = new wxBoxSizer(wxHORIZONTAL);
        szListButtons->Add(new wxButton(GetContainer(), ID_AddFiles, _("Add files...")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szListButtons->Add(new wxButton(GetContainer(), ID_Sort, _("Sort"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szListButtons->Add(new wxButton(GetContainer(), ID_RemoveAll, _("Remove all"), wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szContents->Add(szListButtons, 0, wxALIGN_LEFT | wxALL, BORDER);


        m_FileList.Create(GetContainer(), ID_FileList, _("Input image files"), wxDefaultPosition, wxDefaultSize, wxEL_ALLOW_DELETE);
        szContents->Add(&m_FileList, 1, wxALIGN_CENTER | wxGROW | wxALL, BORDER);

        wxSizer* szProcSettings = new wxBoxSizer(wxHORIZONTAL);

        wxCheckBox* cb = new wxCheckBox(GetContainer(), ID_SubpixelAlignment, _("Sub-pixel alignment"));
        cb->SetValue(true);
        cb->SetToolTip(_("Enable sub-pixel alignment for smoother motion and less drift. Saving of output files will be somewhat slower; sharp, 1-pixel details (if present) may get very slightly blurred"));
        cb->SetValidator(wxGenericValidator(&m_Parameters.subpixelAlignment));
        szProcSettings->Add(cb, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szContents->Add(szProcSettings, 0, wxALIGN_LEFT | wxALL, BORDER);

        wxSizer* szCrop = new wxBoxSizer(wxHORIZONTAL);
            wxArrayString cropChoices;
            cropChoices.Add(_("Crop to intersection"));
            cropChoices.Add(_("Pad to bounding box"));
            wxRadioBox* rb = new wxRadioBox(GetContainer(), ID_Crop, wxEmptyString, wxDefaultPosition, wxDefaultSize, cropChoices, 0, wxRA_SPECIFY_ROWS);
            rb->SetValidator(wxGenericValidator(reinterpret_cast<int*>(&m_Parameters.cropMode)));
            szCrop->Add(rb, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
            szCrop->Add(m_CropBitmapCtrl = new wxGenericStaticBitmap(GetContainer(), ID_CropBitmap, m_CropBitmaps[static_cast<size_t>(CropMode::CROP_TO_INTERSECTION)]),
                1, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szContents->Add(szCrop, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxGROW | wxLEFT | wxRIGHT, BORDER);

        wxSizer* szMethod = new wxBoxSizer(wxHORIZONTAL);
            wxArrayString methodChoices;
            methodChoices.Add(_("Stabilize high-contrast features"));
            methodChoices.Add(_("Align on the solar limb"));
            rb = new wxRadioBox(GetContainer(), ID_Method, wxEmptyString, wxDefaultPosition, wxDefaultSize, methodChoices, 0, wxRA_SPECIFY_ROWS);
            rb->SetValidator(wxGenericValidator(reinterpret_cast<int*>(&m_Parameters.alignmentMethod)));
            szMethod->Add(rb, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

            szMethod->Add(m_AlignMethodTextCtrl = new wxStaticText(GetContainer(), wxID_ANY, GetAlignmentMethodDescription(AlignmentMethod::PHASE_CORRELATION)), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

        szContents->Add(szMethod, 0, wxALIGN_LEFT | wxALIGN_CENTER_VERTICAL | wxGROW | wxLEFT | wxRIGHT, BORDER);

        wxSizer* szOutputDir = new wxBoxSizer(wxHORIZONTAL);
        szOutputDir->Add(new wxStaticText(GetContainer(), wxID_ANY, _("Output folder:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szOutputDir->Add(m_OutputDirCtrl = new wxDirPickerCtrl(GetContainer(), ID_OutputDir, Configuration::AlignOutputPath, _("Select output folder")),
            1, wxALIGN_CENTER_VERTICAL | wxGROW | wxALL, BORDER);
        szContents->Add(szOutputDir, 0, wxGROW | wxALL, BORDER);

        szContents->Add(new wxStaticText(GetContainer(), wxID_ANY,
            _("Processed files will be saved under names appended with \"_aligned\".\nNumber of channels and bit depth will be preserved.")),
            0, wxALIGN_LEFT | wxALL, BORDER);

        AssignContainerSizer(szContents);

    GetTopSizer()->Add(new wxStaticLine(this), 0, wxGROW | wxALL);

    wxSizer* szButtons = new wxBoxSizer(wxHORIZONTAL);
    szButtons->Add(new wxButton(this, ID_Start, _("Start processing")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szButtons->Add(new wxButton(this, wxID_CANCEL), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    GetTopSizer()->Add(szButtons, 0, wxALIGN_RIGHT | wxALL, BORDER);
}

/// Displays the image alignment parameters dialog and receives parameters' values. Returns 'true' if the user clicks "Start processing".
bool GetAlignmentParameters(wxWindow* parent,
    AlignmentParameters_t& params ///< Receives alignment parameters
)
{
    c_ImageAlignmentParams dlg(parent);
    wxRect r = Configuration::AlignParamsDialogPosSize;
    dlg.SetPosition(r.GetPosition());
    dlg.SetSize(r.GetSize());

    FixWindowPosition(dlg);

    bool result = (dlg.ShowModal() == wxID_OK);
    if (result)
    {
        dlg.GetAlignmentParameters(params);
        Configuration::AlignOutputPath = params.outputDir;
    }
    Configuration::AlignParamsDialogPosSize = wxRect(dlg.GetPosition(), dlg.GetSize());

    return result;
}
