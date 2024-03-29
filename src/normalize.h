/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2022 Filip Szczerek <ga.software@yahoo.com>

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
    Normalization dialog header.
*/

#ifndef IMPPG_NORMALIZE_DIALOG_HEADER
#define IMPPG_NORMALIZE_DIALOG_HEADER

#include <wx/dialog.h>
#include <wx/event.h>
#include <wx/textctrl.h>


class c_NormalizeDialog: public wxDialog
{
public:
    c_NormalizeDialog(wxWindow* parent, bool normalizationEnabled, float minLevel, float maxLevel);

    bool IsNormalizationEnabled() { return m_NormalizationEnabled; }
    double GetMinLevel() { return m_MinLevelPercent/100.0f; }
    double GetMaxLevel() { return m_MaxLevelPercent/100.0f; }


    DECLARE_EVENT_TABLE()

private:
    void InitControls();

    void OnCommandEvent(wxCommandEvent& event);

    bool m_NormalizationEnabled{false};
    double m_MinLevelPercent{0};
    double m_MaxLevelPercent{100.0};

    wxTextCtrl* m_MinLevelPercentCtrl{nullptr};
    wxTextCtrl* m_MaxLevelPercentCtrl{nullptr};
};

#endif
