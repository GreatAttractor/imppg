/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2017 Filip Szczerek <ga.software@yahoo.com>

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

#include <wx/window.h>
#include <wx/string.h>
#include <wx/arrstr.h>

enum class CropMode
{
    CROP_TO_INTERSECTION = 0,
    PAD_TO_BOUNDING_BOX = 1,

    NUM // this has to be the last element
};

enum class AlignmentMethod
{
    PHASE_CORRELATION = 0,
    LIMB = 1,

    NUM // this has to be the last element
};

typedef struct
{
    wxArrayString inputFiles;
    AlignmentMethod alignmentMethod;
    bool subpixelAlignment;
    CropMode cropMode;
    wxString outputDir;
} AlignmentParameters_t;

enum class AlignmentAbortReason
{
    USER_REQUESTED, ///< Abort requested by user
    PROC_ERROR ///< Abort due to processing error
};

typedef struct
{
    float radius;
    struct
    {
        float x, y;
    } translation;
} AlignmentEventPayload_t;

/// IDs of events sent from the alignment worker thread
enum
{
    /// Phase correlation method: determined translation of an image relative to its predecessor. Image number: event.GetInt(), also contains a AlignmentEventPayload_t payload
    EID_PHASECORR_IMG_TRANSLATION = wxID_HIGHEST,
    EID_LIMB_FOUND_DISC_RADIUS, ///< Image index: event.GetInt()
    EID_LIMB_USING_RADIUS,
    EID_LIMB_STABILIZATION_PROGRESS,
    EID_LIMB_STABILIZATION_FAILURE,
    EID_SAVED_OUTPUT_IMAGE,     ///< Image index: event.GetInt()
    EID_COMPLETED,              ///< Processing completed
    EID_ABORTED                 ///< Processing aborted; abort reason (AlignmentAbortReason_t): event.getId(); abort message: event.GetString()
};

/// Displays the image alignment parameters dialog and receives parameters' values. Returns 'true' if the user clicks "Start processing".
bool GetAlignmentParameters(wxWindow *parent,
                           AlignmentParameters_t &params ///< Receives alignment parameters
                           );

/// Displays the alignment progress dialog and starts processing. Returns 'true' if processing completes.
bool AlignImages(wxWindow *parent, AlignmentParameters_t &params);

#endif // IMPPG_IMAGE_ALIGNMENT_HEADER
