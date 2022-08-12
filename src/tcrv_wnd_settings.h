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
    Tone curve window settings dialog header.
*/

#ifndef IMPPG_TCRV_SETTINGS_DIALOG_HEADER
#define IMPPG_TCRV_SETTINGS_DIALOG_HEADER

#include <wx/clrpicker.h>
#include <wx/dialog.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/spinctrl.h>

#include "common/common.h"


class c_ToneCurveWindowSettingsDialog: public wxDialog
{
    void InitControls();
    void SetColors(ToneCurveEditorColors colorSet);

    void EndModal(int retCode) override;

    wxPanel* m_PnColors{nullptr};
    wxRadioBox* m_RbColorSet{nullptr};
    wxColourPickerCtrl* m_CurveColor{nullptr};
    wxColourPickerCtrl* m_CurvePointColor{nullptr};
    wxColourPickerCtrl* m_SelectedCurvePointColor{nullptr};
    wxColourPickerCtrl* m_HistogramColor{nullptr};
    wxColourPickerCtrl* m_BackgroundColor{nullptr};
    wxSpinCtrl* m_CurveWidth{nullptr};
    wxSpinCtrl* m_CurvePointSize{nullptr};

public:
    c_ToneCurveWindowSettingsDialog(wxWindow* parent);
};

#endif // IMPPG_TCRV_SETTINGS_DIALOG_HEADER
