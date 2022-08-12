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
    Tone curve window settings dialog implementation.
*/

#include <tuple>
#include <wx/gdicmn.h>
#include <wx/radiobox.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "appconfig.h"
#include "common/common.h"
#include "tcrv_wnd_settings.h"


const int BORDER = 5; ///< Border size (in pixels between) controls

c_ToneCurveWindowSettingsDialog::c_ToneCurveWindowSettingsDialog(wxWindow* parent)
: wxDialog(parent, wxID_ANY, _("Tone Curve Editor"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    InitControls();

    wxRect r = Configuration::ToneCurveSettingsDialogPosSize;
    SetPosition(r.GetPosition());
    SetSize(r.GetSize());
    FixWindowPosition(*this);
}

void c_ToneCurveWindowSettingsDialog::SetColors(ToneCurveEditorColors colorSet)
{
    m_RbColorSet->SetSelection(static_cast<int>(colorSet));

    switch (colorSet)
    {
        case ToneCurveEditorColors::ImPPGDefaults:
            m_CurveColor->SetColour(DefaultColors::ImPPG::Curve);
            m_CurvePointColor->SetColour(DefaultColors::ImPPG::CurvePoint);
            m_SelectedCurvePointColor->SetColour(DefaultColors::ImPPG::SelectedCurvePoint);
            m_HistogramColor->SetColour(DefaultColors::ImPPG::Histogram);
            m_BackgroundColor->SetColour(DefaultColors::ImPPG::CurveBackground);
            break;

        case ToneCurveEditorColors::SystemDefaults:
            m_CurveColor->SetColour(DefaultColors::System::Curve());
            m_CurvePointColor->SetColour(DefaultColors::System::CurvePoint());
            m_SelectedCurvePointColor->SetColour(DefaultColors::System::SelectedCurvePoint());
            m_HistogramColor->SetColour(DefaultColors::System::Histogram());
            m_BackgroundColor->SetColour(DefaultColors::System::CurveBackground());
            break;

        case ToneCurveEditorColors::Custom:
            m_CurveColor->SetColour(Configuration::ToneCurveEditor_CurveColor);
            m_CurvePointColor->SetColour(Configuration::ToneCurveEditor_CurvePointColor);
            m_SelectedCurvePointColor->SetColour(Configuration::ToneCurveEditor_SelectedCurvePointColor);
            m_HistogramColor->SetColour(Configuration::ToneCurveEditor_HistogramColor);
            m_BackgroundColor->SetColour(Configuration::ToneCurveEditor_BackgroundColor);
            break;

        default: break;
    }
}

void c_ToneCurveWindowSettingsDialog::InitControls()
{
    auto* szTop = new wxBoxSizer(wxVERTICAL);

    const wxString colorOptions[] = { _("ImPPG defaults"), _("System defaults"), _("Custom") };
    m_RbColorSet = new wxRadioBox(this, wxID_ANY, _("Choose tone curve editor colors"), wxDefaultPosition, wxDefaultSize, 3, colorOptions, 0, wxRA_SPECIFY_ROWS);
    m_RbColorSet->SetSelection(static_cast<int>(static_cast<ToneCurveEditorColors>(Configuration::ToneCurveColors)));
    m_RbColorSet->Bind(wxEVT_RADIOBOX, [this](wxCommandEvent& evt) { SetColors(static_cast<ToneCurveEditorColors>(evt.GetSelection())); });
    szTop->Add(m_RbColorSet, 0, wxGROW | wxALL, BORDER);

    this->m_PnColors = new wxPanel(this, wxID_ANY);
    auto* szColors = new wxBoxSizer(wxVERTICAL);
    // create color picker controls by iterating over tuples of: (label; control ptr to initialize; color to use if "custom" color set is active)
    for (const auto& i: std::initializer_list<std::tuple<wxString, wxColourPickerCtrl*&, wxColour>>
        { { _("Curve:"),                m_CurveColor,              Configuration::ToneCurveEditor_CurveColor },
          { _("Curve point:"),          m_CurvePointColor,         Configuration::ToneCurveEditor_CurvePointColor },
          { _("Selected curve point:"), m_SelectedCurvePointColor, Configuration::ToneCurveEditor_SelectedCurvePointColor },
          { _("Histogram:"),            m_HistogramColor,          Configuration::ToneCurveEditor_HistogramColor },
          { _("Background:"),           m_BackgroundColor,         Configuration::ToneCurveEditor_BackgroundColor } })
    {
        auto* sz = new wxBoxSizer(wxHORIZONTAL);
        sz->Add(new wxStaticText(m_PnColors, wxID_ANY, std::get<0>(i)), 1, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        auto*& colorPicker = std::get<1>(i);
        colorPicker = new wxColourPickerCtrl(m_PnColors, wxID_ANY);
        colorPicker->Bind(wxEVT_COLOURPICKER_CHANGED, [this](wxColourPickerEvent&) { m_RbColorSet->SetSelection(static_cast<int>(ToneCurveEditorColors::Custom)); });
        if (Configuration::ToneCurveColors == ToneCurveEditorColors::Custom)
        {
            colorPicker->SetColour(std::get<2>(i));
        }
        sz->Add(colorPicker, 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
        szColors->Add(sz, 0, wxGROW | wxALL, BORDER);
    }
    m_PnColors->SetSizer(szColors);
    szTop->Add(m_PnColors, 0, wxGROW | wxALL, BORDER);

    SetColors(Configuration::ToneCurveColors);

    auto szCurveWidth = new wxBoxSizer(wxHORIZONTAL);
    szCurveWidth->Add(new wxStaticText(this, wxID_ANY, _("Curve width:")), 1, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szCurveWidth->Add(m_CurveWidth = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 50, 1), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    m_CurveWidth->SetValue(Configuration::ToneCurveEditor_CurveWidth);
    szTop->Add(szCurveWidth, 0, wxGROW | wxALL, BORDER);

    auto szCurvePoint = new wxBoxSizer(wxHORIZONTAL);
    szCurvePoint->Add(new wxStaticText(this, wxID_ANY, _("Curve point size:")), 1, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    szCurvePoint->Add(m_CurvePointSize = new wxSpinCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 4, 50, 4), 0, wxALIGN_CENTER_VERTICAL | wxALL, BORDER);
    m_CurvePointSize->SetValue(Configuration::ToneCurveEditor_CurvePointSize);
    szTop->Add(szCurvePoint, 0, wxGROW | wxALL, BORDER);

    szTop->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxGROW | wxALL, BORDER);

    SetSizer(szTop);
    Fit();
}

void c_ToneCurveWindowSettingsDialog::EndModal(int retCode)
{
    if (wxID_OK == retCode)
    {
        Configuration::ToneCurveEditor_CurveWidth = m_CurveWidth->GetValue();
        Configuration::ToneCurveEditor_CurvePointSize = m_CurvePointSize->GetValue();
        const auto colorSet = static_cast<ToneCurveEditorColors>(m_RbColorSet->GetSelection());
        Configuration::ToneCurveColors = colorSet;

        if (colorSet == ToneCurveEditorColors::Custom)
        {
            Configuration::ToneCurveEditor_CurveColor = m_CurveColor->GetColour();
            Configuration::ToneCurveEditor_BackgroundColor = m_BackgroundColor->GetColour();
            Configuration::ToneCurveEditor_CurvePointColor = m_CurvePointColor->GetColour();
            Configuration::ToneCurveEditor_SelectedCurvePointColor = m_SelectedCurvePointColor->GetColour();
            Configuration::ToneCurveEditor_HistogramColor = m_HistogramColor->GetColour();
        }
    }

    Configuration::ToneCurveSettingsDialogPosSize = wxRect(GetPosition(), GetSize());

    wxDialog::EndModal(retCode);
}