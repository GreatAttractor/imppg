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
    Batch processing header.
*/

#ifndef IMPPG_BATCH_H
#define IMPPG_BATCH_H

#include <wx/window.h>

/// Allows the user to choose images to process, choose the processing settings to use and displays the batch processing progress and control dialog
void BatchProcessing(
    wxWindow *parent ///< Pointer to the application's main window
);




#endif
