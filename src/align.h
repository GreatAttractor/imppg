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
    Image alignment header.
*/

#ifndef IMPPG_IMAGE_ALIGNMENT_HEADER
#define IMPPG_IMAGE_ALIGNMENT_HEADER

#include "align_proc.h"

#include <wx/window.h>
#include <wx/string.h>
#include <wx/arrstr.h>

/// Displays the image alignment parameters dialog and receives parameters' values. Returns 'true' if the user clicks "Start processing".
bool GetAlignmentParameters(
    wxWindow* parent,
    AlignmentParameters_t& params ///< Receives alignment parameters
);

/// Displays the alignment progress dialog and starts processing. Returns 'true' if processing completes.
bool AlignImages(wxWindow* parent, AlignmentParameters_t& params);

#endif // IMPPG_IMAGE_ALIGNMENT_HEADER
