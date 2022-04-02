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
    Normalization dialog implementation.
*/

#include "ctrl_ids.h"
#include "normalize.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/valgen.h>


const int BORDER = 5; ///< Border size (in pixels between) controls

BEGIN_EVENT_TABLE(c_NormalizeDialog, wxDialog)
    EVT_CHECKBOX(ID_NormalizationEnabled, c_NormalizeDialog::OnCommandEvent)
    EVT_BUTTON(wxID_OK, c_NormalizeDialog::OnCommandEvent)
END_EVENT_TABLE()

c_NormalizeDialog::c_NormalizeDialog(wxWindow* parent, bool normalizationEnabled, float minLevel, float maxLevel)
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
        m_MinLevelPercentCtrl->Disable();
        m_MaxLevelPercentCtrl->Disable();
    }
}

void c_NormalizeDialog::OnCommandEvent(wxCommandEvent& event)
{
    switch (event.GetId())
    {
    case ID_NormalizationEnabled:
        m_MinLevelPercentCtrl->Enable(event.IsChecked());
        m_MaxLevelPercentCtrl->Enable(event.IsChecked());
        TransferDataFromWindow();
        break;

    case wxID_OK:
        if (m_NormalizationEnabled)
        {
            double minVal{}, maxVal{};
            // not writing directly to `m_Min/MaxLevelPercentCtrl` in case only a part of a string got parsed
            if (!m_MinLevelPercentCtrl->GetValue().ToDouble(&minVal) ||
                !m_MaxLevelPercentCtrl->GetValue().ToDouble(&maxVal))
            {
                wxMessageBox(_("Invalid number."), _("Error"), wxICON_ERROR, this);
            }
            else
            {
                m_MinLevelPercent = minVal;
                m_MaxLevelPercent = maxVal;
                event.Skip();
            }
        }
        else
        {
            event.Skip();
        }
        break;
    }
}

void c_NormalizeDialog::InitControls()
{
    wxSizer* szTop = new wxBoxSizer(wxVERTICAL);

    szTop->Add(new wxStaticText(this, wxID_ANY,
        _("Normalization of image brightness is performed prior\n"
		  "to all other processing steps.")),
        0, wxALIGN_CENTER_VERTICAL | wxGROW | wxALL, BORDER);

    szTop->Add(new wxCheckBox(this, ID_NormalizationEnabled, _("Normalization enabled"), wxDefaultPosition, wxDefaultSize, 0, wxGenericValidator(&m_NormalizationEnabled)),
        0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

    wxSizer* szMin = new wxBoxSizer(wxHORIZONTAL);
    szMin->Add(new wxStaticText(this, wxID_ANY, _("Set the darkest input pixels to:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

    m_MinLevelPercentCtrl = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.1f", m_MinLevelPercent));
    szMin->Add(m_MinLevelPercentCtrl, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

    szMin->Add(new wxStaticText(this, wxID_ANY, "%"), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szTop->Add(szMin, 0, wxGROW | wxALL, BORDER);

    wxSizer* szMax = new wxBoxSizer(wxHORIZONTAL);
    szMax->Add(new wxStaticText(this, wxID_ANY, _("Set the brightest input pixels to:")), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);

    m_MaxLevelPercentCtrl = new wxTextCtrl(this, wxID_ANY, wxString::Format("%.1f", m_MaxLevelPercent));
    szMax->Add(m_MaxLevelPercentCtrl, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5);

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
