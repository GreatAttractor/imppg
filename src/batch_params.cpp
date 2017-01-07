/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2017 Filip Szczerek <ga.software@yahoo.com>

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
    Batch processing parameters dialog.
*/

#include <cassert>
#include <wx/msgdlg.h>
#include <wx/arrstr.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/filepicker.h>
#include <wx/choice.h>
#include <wx/statline.h>
#include "appconfig.h"
#include "common.h"
#include "batch_params.h"
#include "image.h"

enum
{
    ID_AddFiles = wxID_HIGHEST,
    ID_Start,
    ID_RemoveSelected,
    ID_FileList,
    ID_SettingsFilePicker,
    ID_SettingsFile,
    ID_OutputDir,
    ID_OutputFormat
};

const int BORDER = 5; ///< Border size (in pixels) between controls

BEGIN_EVENT_TABLE(c_BatchParamsDialog, wxDialog)
    EVT_BUTTON(ID_AddFiles, c_BatchParamsDialog::OnCommandEvent)
    EVT_BUTTON(ID_Start, c_BatchParamsDialog::OnCommandEvent)
    EVT_BUTTON(wxID_CANCEL, c_BatchParamsDialog::OnCommandEvent)
    EVT_BUTTON(ID_RemoveSelected, c_BatchParamsDialog::OnCommandEvent)
    EVT_DIRPICKER_CHANGED(ID_OutputDir, c_BatchParamsDialog::OnOutputDirChanged)
    EVT_FILEPICKER_CHANGED(ID_SettingsFilePicker, c_BatchParamsDialog::OnSettingsFileChanged)
END_EVENT_TABLE()

const wxArrayString c_BatchParamsDialog::GetInputFileNames()
{
    return ((wxListBox *)FindWindowById(ID_FileList, this))->GetStrings();
}

wxString c_BatchParamsDialog::GetOutputDirectory()
{
    return ((wxDirPickerCtrl *)FindWindowById(ID_OutputDir, this))->GetPath();
}

OutputFormat_t c_BatchParamsDialog::GetOutputFormat()
{
    return (OutputFormat_t)((wxChoice *)FindWindowById(ID_OutputFormat, this))->GetCurrentSelection();
}

wxString c_BatchParamsDialog::GetSettingsFileName()
{
    return ((wxFilePickerCtrl *)FindWindowById(ID_SettingsFilePicker, this))->GetPath();
}


void c_BatchParamsDialog::OnSettingsFileChanged(wxFileDirPickerEvent &event)
{
    ((wxStaticText *)FindWindowById(ID_SettingsFile, this))->SetLabel(event.GetPath());
}

void c_BatchParamsDialog::OnOutputDirChanged(wxFileDirPickerEvent &event)
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
            ((wxDirPickerCtrl *)FindWindowById(ID_OutputDir, this))->SetPath(path.GetFullPath());
        // else: do nothing, apparently the user is editing the path manually, so it's temporarily incorrect
    }
#endif
}

void c_BatchParamsDialog::StoreConfiguration()
{
    Configuration::BatchLoadSettingsPath = wxFileName(((wxFilePickerCtrl *)FindWindowById(ID_SettingsFilePicker, this))->GetPath()).GetPath();
    Configuration::BatchDialogPosSize = wxRect(GetPosition(), GetSize());
    Configuration::BatchOutputPath = ((wxDirPickerCtrl *)FindWindowById(ID_OutputDir, this))->GetPath();
    Configuration::BatchOutputFormat = (OutputFormat_t)((wxChoice *)FindWindowById(ID_OutputFormat, this))->GetSelection();
}

void c_BatchParamsDialog::OnCommandEvent(wxCommandEvent &event)
{
    switch (event.GetId())
    {
    case ID_Start:
        if (!wxFileName::DirExists(((wxDirPickerCtrl *)FindWindowById(ID_OutputDir, this))->GetPath()))
            wxMessageBox(_("Selected output folder does not exist."), _("Error"), wxICON_ERROR, this);
        else if (m_FileList->GetCount() == 0)
            wxMessageBox(_("No input files selected."), _("Error"), wxICON_ERROR, this);
        else if (!wxFileName::FileExists(((wxFilePickerCtrl *)FindWindowById(ID_SettingsFilePicker, this))->GetPath()))
            wxMessageBox(_("Settings file not specified or does not exist."), _("Error"), wxICON_ERROR, this);
        else
        {
            StoreConfiguration();
            EndModal(wxID_OK);
        }
        break;

    case wxID_CANCEL:
        StoreConfiguration();
        EndModal(wxID_CANCEL);
        break;

    case ID_AddFiles:
        {
            wxFileDialog fileDlg(this, _("Choose input file(s)"), Configuration::BatchFileOpenPath, wxEmptyString, INPUT_FILE_FILTERS, wxFD_OPEN | wxFD_MULTIPLE);
            if (fileDlg.ShowModal() == wxID_OK)
            {
                Configuration::BatchFileOpenPath = wxFileName(fileDlg.GetPath()).GetPath();

                wxArrayString files;
                fileDlg.GetPaths(files);
                for (size_t i = 0; i < files.Count(); i++)
                {
                    m_FileList->Append(files[i]);
                }
            }

            break;
        }

    case ID_RemoveSelected:
        for (unsigned i = 0; i < m_FileList->GetCount(); i++)
            if (m_FileList->IsSelected(i))
                m_FileList->Delete(i);
        break;
    }
}

c_BatchParamsDialog::c_BatchParamsDialog(wxWindow *parent)
: c_ScrollableDialog(parent, wxID_ANY, _("Batch processing"))
{
    InitControls(BORDER);
    wxRect r = Configuration::BatchDialogPosSize;
    SetPosition(r.GetPosition());
    SetSize(r.GetSize());
    FixWindowPosition(*this);
}

void c_BatchParamsDialog::DoInitControls()
{
    wxSizer *szTop = new wxBoxSizer(wxVERTICAL);

    szTop->Add(new wxButton(GetContainer(), ID_AddFiles, _("Add files...")), 0, wxALIGN_LEFT | wxALL, BORDER);

    m_FileList = new wxListBox(GetContainer(), ID_FileList, wxDefaultPosition, wxDefaultSize, wxArrayString(), wxLB_EXTENDED);
    szTop->Add(m_FileList, 1, wxALIGN_CENTER | wxGROW | wxALL, BORDER);

    wxSizer *szRemoveSel = new wxBoxSizer(wxHORIZONTAL);
    szRemoveSel->Add(new wxButton(GetContainer(), ID_RemoveSelected, _("Remove selected from list")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szRemoveSel->Add(new wxStaticText(GetContainer(), wxID_ANY, _("Use a click, Shift+click, Ctrl+click or Shift+up/down to select list items."),
        wxDefaultPosition, wxDefaultSize),
        0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szTop->Add(szRemoveSel, 0, wxALIGN_LEFT | wxALL, BORDER);

    wxSizer *szProcSettings = new wxBoxSizer(wxHORIZONTAL);
    szProcSettings->Add(new wxStaticText(GetContainer(), wxID_ANY, _("Settings file:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szProcSettings->Add(new wxFilePickerCtrl(GetContainer(), ID_SettingsFilePicker, "", _("Choose the file with processing settings"),
        _("XML files (*.xml)") + "|*.xml", wxDefaultPosition, wxDefaultSize, wxFD_OPEN | wxFD_FILE_MUST_EXIST),
        0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    ((wxFilePickerCtrl *)FindWindowById(ID_SettingsFilePicker, GetContainer()))->SetInitialDirectory(Configuration::BatchLoadSettingsPath);
    szProcSettings->Add(new wxStaticText(GetContainer(), ID_SettingsFile,
        _("Select the XML file with processing settings."),
        wxDefaultPosition, wxDefaultSize),
        0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szTop->Add(szProcSettings, 0, wxALIGN_LEFT | wxALL, BORDER);

    szTop->Add(new wxStaticText(GetContainer(), wxID_ANY, _("A settings file can be created using the main window's \"Save processing settings\" function.")),
        0, wxALIGN_LEFT | wxALL, BORDER);

    wxSizer *szOutputDir = new wxBoxSizer(wxHORIZONTAL);
    szOutputDir->Add(new wxStaticText(GetContainer(), wxID_ANY, _("Output folder:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szOutputDir->Add(new wxDirPickerCtrl(GetContainer(), ID_OutputDir, Configuration::BatchOutputPath, _("Select output folder")),
        1, wxALIGN_CENTER_VERTICAL | wxGROW | wxALL, BORDER);
    szTop->Add(szOutputDir, 0, wxGROW | wxALL, BORDER);

    szTop->Add(new wxStaticText(GetContainer(), wxID_ANY, _("Processed files will be saved under names appended with \"_out\".")), 0, wxALIGN_LEFT | wxALL, BORDER);

    wxSizer *szOutFmt = new wxBoxSizer(wxHORIZONTAL);
    szOutFmt->Add(new wxStaticText(GetContainer(), wxID_ANY, _("Output format:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    wxArrayString formatStr;
    for (int i = 0; i < OUTF_LAST; i++)
        formatStr.Add(GetOutputFormatDescription((OutputFormat_t)i));
    wxChoice *outFormats = new wxChoice(GetContainer(), ID_OutputFormat, wxDefaultPosition, wxDefaultSize, formatStr);
    outFormats->SetSelection((int)Configuration::BatchOutputFormat);
    szOutFmt->Add(outFormats, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szTop->Add(szOutFmt, 0, wxALIGN_LEFT | wxALL, BORDER);

    AssignContainerSizer(szTop);

    GetTopSizer()->Add(new wxStaticLine(this), 0, wxGROW | wxALL, BORDER);

    wxSizer *szButtons = new wxBoxSizer(wxHORIZONTAL);
    szButtons->Add(new wxButton(this, ID_Start, _("Start processing")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szButtons->Add(new wxButton(this, wxID_CANCEL), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    GetTopSizer()->Add(szButtons, 0, wxALIGN_RIGHT | wxALL, BORDER);
}
