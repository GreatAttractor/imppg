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
    Batch processing parameters dialog header.
*/

#include <wx/arrstr.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/filepicker.h>
#include <wx/grid.h>
#include <wx/listbox.h>

#include "image.h"
#include "scrollable_dlg.h"

class c_BatchParamsDialog : public c_ScrollableDialog
{
    void OnCommandEvent(wxCommandEvent& event);
    void OnOutputDirChanged(wxFileDirPickerEvent& event);
    void OnSettingsFileChanged(wxFileDirPickerEvent& event);

    void DoInitControls() override;
    void StoreConfiguration();

    /// Lists the chosen input files
    wxListBox* m_FileList{nullptr};
    wxDirPickerCtrl* m_OutputDirCtrl{nullptr};
    wxChoice* m_OutputFormatsCtrl{nullptr};
    wxFilePickerCtrl* m_SettingsFileCtrl{nullptr};

public:
    c_BatchParamsDialog(wxWindow* parent);

    const wxArrayString GetInputFileNames();
    wxString GetOutputDirectory();
    OutputFormat GetOutputFormat();
    wxString GetSettingsFileName();

    DECLARE_EVENT_TABLE()
};
