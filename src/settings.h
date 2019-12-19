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
    Processing settings functions header.
*/

#ifndef IMPGG_PRESET_H
#define IMPGG_PRESET_H

#include "proc_settings.h"
#include "tcrv.h"

/// Saves the settings of Lucy-Richardson deconvolution, unsharp masking, tone curve and deringing; returns 'false' on error.
bool SaveSettings(wxString filePath, const ProcessingSettings& settings);

/// Loads the settings of Lucy-Richardson deconvolution, unsharp masking, tone curve and deringing; returns 'false' on error.
/** If the specified file does not contain some of the settings, the corresponding parameters' values will not be updated.*/
bool LoadSettings(
    wxString filePath,
    ProcessingSettings& settings,
    bool* loadedLR = nullptr,
    bool* loadedUnsh = nullptr,
    bool* loadedTCurve = nullptr
);

#endif
