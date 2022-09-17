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

#include "progress_bar.h"

#include <wx/dcclient.h>
#include <wx/settings.h>

c_ProgressBar::c_ProgressBar(wxWindow* parent)
: wxWindow(parent, wxID_ANY)
{
    Bind(wxEVT_PAINT, &c_ProgressBar::OnPaint, this);
}

void c_ProgressBar::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(this);

    const auto size = GetClientSize();

    dc.SetBrush(wxBrush(wxSystemSettings::GetColour(/*wxSYS_COLOUR_BTNFACE*/wxSYS_COLOUR_HIGHLIGHT)));
    dc.DrawRectangle(0, 0, size.x / 2, size.y);
    dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_CAPTIONTEXT));
    const auto text = wxString::Format("%d%%", 47);
    const wxSize textExtent = GetTextExtent(text);
    dc.DrawText(text, (size.x - textExtent.x) / 2, (size.y - textExtent.y) / 2);
}
