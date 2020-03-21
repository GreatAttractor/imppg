/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2020 Filip Szczerek <ga.software@yahoo.com>

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
    Advanced settings dialog implementation.
*/

#include "appconfig.h"

#include <wx/checkbox.h>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

constexpr int BORDER = 5; ///< Border size (in pixels between) controls

class c_AdvancedSettingsDialog: public wxDialog
{
    void InitControls();

    struct
    {
        wxCheckBox* normalizeFits{nullptr};
    } m_Ctrls;

public:
    c_AdvancedSettingsDialog(wxWindow* parent);

    void SaveSettings();
};

c_AdvancedSettingsDialog::c_AdvancedSettingsDialog(wxWindow* parent)
: wxDialog(
    parent,
    wxID_ANY,
    _("Advanced settings"),
    wxDefaultPosition,
    wxDefaultSize,
    wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
)
{
    InitControls();
}

void c_AdvancedSettingsDialog::SaveSettings()
{
    Configuration::NormalizeFITSValues = m_Ctrls.normalizeFits->GetValue();
}

void c_AdvancedSettingsDialog::InitControls()
{
    wxSizer* szTop = new wxBoxSizer(wxVERTICAL);

    m_Ctrls.normalizeFits = new wxCheckBox(this, wxID_ANY, _("Normalize FITS pixel values"));
    m_Ctrls.normalizeFits->SetValue(Configuration::NormalizeFITSValues);
    szTop->Add(m_Ctrls.normalizeFits, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szTop->Add(new wxStaticText(this, wxID_ANY,
        _("Enables normalization of floating-point pixel values of FITS images; the highest value becomes 1.0.")),
        0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER
    );

    szTop->AddStretchSpacer();

    szTop->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxGROW | wxALL, BORDER);

    SetSizer(szTop);
    Fit();
}

void ShowAdvancedSettingsDialog(wxWindow* parent)
{
    c_AdvancedSettingsDialog dlg{parent};
    if (dlg.ShowModal() == wxID_OK)
    {
        dlg.SaveSettings();
    }
}
