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
    Numerical value formatter.
*/

#ifndef IMPPG_NUM_FORMATTER_H
#define IMPPG_NUM_FORMATTER_H

#include <optional>
#include <vector>
#include <wx/string.h>


namespace NumFormatter
{

/// Formats value using "classic" locale (decimal separator = '.').
wxString Format(double value, unsigned numDecimals);

/// Parses value, accepts '.' and ',' as decimal separator; returns `false` on error.
bool Parse(wxString s, float& value);

/// Parses value list, accepts '.' and ',' as decimal separator; returns `false` on error.
bool ParseList(wxString s, std::vector<float>& values, char separator);

}

#endif // IMPPG_NUM_FORMATTER_H
