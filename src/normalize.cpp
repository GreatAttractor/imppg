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
    Normalization dialog implementation.
*/

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/valnum.h>
#include <wx/valgen.h>
#include <wx/checkbox.h>
#include "normalize.h"
#include "ctrl_ids.h"

const int BORDER = 5; ///< Border size (in pixels between) controls

BEGIN_EVENT_TABLE(c_NormalizeDialog, wxDialog)
    EVT_CLOSE(c_NormalizeDialog::OnClose)
    EVT_CHECKBOX(ID_NormalizationEnabled, c_NormalizeDialog::OnCommandEvent)
END_EVENT_TABLE()

c_NormalizeDialog::c_NormalizeDialog(wxWindow *parent, bool normalizationEnabled, float minLevel, float maxLevel)
: wxDialog(parent, wxID_ANY, _("Brightness Normalization"), wxDefaultPosition, wxDefaultSize,
        wxDEFAULT_DIALOG_STYLE)
{
    m_NormalizationEnabled = normalizationEnabled;
    m_MinLevelPercent = 100.0f * minLevel;
    m_MaxLevelPercent = 100.0f * maxLevel;

    SetExtraStyle(GetExtraStyle() | wxWS_EX_VALIDATE_RECURSIVELY);

    InitControls();
    TransferDataToWindow();
    if (!m_NormalizationEnabled)
    {
        FindWindowById(ID_MinLevel, this)->Disable();
        FindWindowById(ID_MaxLevel, this)->Disable();
    }
}

void c_NormalizeDialog::OnCommandEvent(wxCommandEvent &event)
{
    switch (event.GetId())
    {
    case ID_NormalizationEnabled:
        FindWindowById(ID_MinLevel, this)->Enable(event.IsChecked());
        FindWindowById(ID_MaxLevel, this)->Enable(event.IsChecked());
        break;
    }
}

void c_NormalizeDialog::OnClose(wxCloseEvent &event)
{
    if (GetReturnCode() == wxID_OK)
        TransferDataFromWindow();
    event.Skip();
}

void c_NormalizeDialog::InitControls()
{
    wxSizer *szTop = new wxBoxSizer(wxVERTICAL);

    szTop->Add(new wxStaticText(this, wxID_ANY,
        _("Normalization of image brightness is performed prior\n"
		  "to all other processing steps.")),
        0, wxALIGN_CENTER_VERTICAL | wxGROW | wxALL, BORDER);
          
    szTop->Add(new wxCheckBox(this, ID_NormalizationEnabled, _("Normalization enabled"), wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&m_NormalizationEnabled)),
        0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

    wxSizer *szMin = new wxBoxSizer(wxHORIZONTAL);
    szMin->Add(new wxStaticText(this, wxID_ANY, _("Set the darkest input pixels to:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szMin->Add(new wxTextCtrl(this, ID_MinLevel, "0.0", wxDefaultPosition, wxDefaultSize, 0, wxFloatingPointValidator<double>(5, &m_MinLevelPercent)),
        0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    szMin->Add(new wxStaticText(this, wxID_ANY, "%"), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szTop->Add(szMin, 0, wxGROW | wxALL, BORDER);

    wxSizer *szMax = new wxBoxSizer(wxHORIZONTAL);
    szMax->Add(new wxStaticText(this, wxID_ANY, _("Set the brightest input pixels to:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szMax->Add(new wxTextCtrl(this, ID_MaxLevel, "100.0", wxDefaultPosition, wxDefaultSize, 0, wxFloatingPointValidator<double>(5, &m_MaxLevelPercent)),
        0, wxALIGN_CENTER_VERTICAL | wxALL, 5);
    szMax->Add(new wxStaticText(this, wxID_ANY, "%"), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szTop->Add(szMax, 0, wxGROW | wxALL, BORDER);

    szTop->Add(new wxStaticText(this, wxID_ANY,
        _("Values below 0% and above 100% are allowed (they may result\n"
		  "in a clipped histogram). The second value may be lower than\n"
          "the first (brightness levels will be inverted).")),
        0, wxALIGN_CENTER_VERTICAL | wxGROW | wxALL, BORDER);

    szTop->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxGROW | wxALL, BORDER);

    SetSizer(szTop);
    Fit();
}
