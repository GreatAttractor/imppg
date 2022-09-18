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
    Progress bar widget implementation.
*/

#include "imppg_assert.h"
#include "progress_bar.h"

#include <wx/dcclient.h>
#include <wx/settings.h>

// private definitions
namespace
{

constexpr unsigned NUM_PULSE_PHASES = 10;

}

c_ProgressBar::c_ProgressBar(wxWindow* parent, unsigned range)
: wxWindow(parent, wxID_ANY), m_Range(range)
{
    Bind(wxEVT_PAINT, &c_ProgressBar::OnPaint, this);
}

void c_ProgressBar::SetValue(unsigned value)
{
    m_Value = value;
    m_Pulsing = false;
    Refresh();
}

void c_ProgressBar::Pulse()
{
    if (!m_Pulsing)
    {
        m_Pulsing = true;
        m_PulsePhase = 0;
    }
    else
    {
        m_PulsePhase += m_PulseDirection;
        if (m_PulsePhase >= NUM_PULSE_PHASES - 1)
        {
            m_PulseDirection = -1;
        }
        else if (0 == m_PulsePhase)
        {
            m_PulseDirection = 1;
        }
    }

    Refresh();
}

void c_ProgressBar::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(this);

    const auto size = GetClientSize();
    dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT)));

    if (m_Pulsing)
    {
        dc.DrawRectangle(
            (m_PulsePhase % NUM_PULSE_PHASES) * size.x / NUM_PULSE_PHASES, 0, size.x / NUM_PULSE_PHASES, size.y
        );
    }
    else
    {
        dc.DrawRectangle(0, 0, size.x * m_Value / m_Range, size.y);
        dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_CAPTIONTEXT));
        const auto text = wxString::Format("%d%%", 100 * m_Value / m_Range);
        const wxSize textExtent = GetTextExtent(text);
        dc.DrawText(text, (size.x - textExtent.x) / 2, (size.y - textExtent.y) / 2);
    }
}
