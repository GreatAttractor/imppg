/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2022 Filip Szczerek <ga.software@yahoo.com>

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
    Progress bar widget header.
*/

#pragma once

#include <wx/window.h>

class c_ProgressBar: public wxWindow
{
public:
    c_ProgressBar(wxWindow* parent, unsigned range);

    void SetValue(unsigned value);

    void Pulse();

private:
    void OnPaint(wxPaintEvent& event);

    unsigned m_Range{100};

    unsigned m_Value{0};

    unsigned m_PulsePhase{0};

    int m_PulseDirection{1};

    bool m_Pulsing{false};
};
