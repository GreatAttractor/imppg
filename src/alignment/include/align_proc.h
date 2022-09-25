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
    Image alignment worker thread header.
*/

#ifndef IMPPG_IMAGE_ALIGNMENT_THREAD_HEADER
#define IMPPG_IMAGE_ALIGNMENT_THREAD_HEADER

#include <vector>
#include <wx/thread.h>
#include <wx/window.h>
#include <wx/arrstr.h>

#include "common/common.h"

enum class CropMode: int
{
    CROP_TO_INTERSECTION = 0,
    PAD_TO_BOUNDING_BOX = 1,

    NUM // this has to be the last element
};

enum class AlignmentMethod: int
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
    bool normalizeFitsValues;
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

class c_ImageAlignmentWorkerThread: public wxThread
{
    wxWindow& m_Parent;

    /// The parent can perform Post() on this semaphore (via AbortProcessing())
    wxSemaphore m_AbortReq;

    bool m_ProcessingCompleted; ///< 'true' if processing has completed
    bool m_ThreadAborted; ///< 'true' if IsAbortRequested() has been called and has returned 'true'
    std::string m_CompletionMessage;
    AlignmentParameters_t m_Parameters;

    /// Arguments: image index and its determined translation vector
    void PhaseCorrImgTranslationCallback(int imgIdx, float Tx, float Ty);
    bool IsAbortRequested();
    void SendMessageToParent(int id, int value = 0, wxString msg = wxEmptyString, AlignmentEventPayload_t* payload = nullptr);

    /// Loads the specified input image, translates it and saves using the specified size and translation vector
    bool SaveTranslatedOutputImage(wxString inputFileName, int outputWidth, int outputHeight,
        float Tx, ///< X offset within the input image
        float Ty  ///< Y offset within the input image
        );

    void PhaseCorrelationAlignment(); ///< Aligns the images by keeping the high-contrast features stationary
    void LimbAlignment(); ///< Aligns the images by keeping the limb stationary

    /// Finds disc radii in input images; returns 'true' on success
    bool FindRadii(
        std::vector<std::vector<FloatPoint_t>>& limbPoints, ///< Receives limb points found in n-th image
        std::vector<float>& radii, ///< Receives disc radii determined for each image
        std::vector<Point_t>& imgSizes, ///< Receives input image sizes
        std::vector<Point_t>& centroids ///< Receives image centroids
    );

    /// Performs the final stabilization phase of limb alignment; returns 'true' on success
    bool StabilizeLimbAlignment(
        /// Translation vectors to be corrected by stabilization
        std::vector<FloatPoint_t>& translation,
        /// Start of the images' intersection, relative to the first image's origin
        Point_t intersectionStart,
        int intrWidth, ///< Images' intersection width
        int intrHeight, ///< Images' intersection height
        wxString& errorMsg ///< Receives error message (if any)
    );


public:
    c_ImageAlignmentWorkerThread(
        wxWindow& parent,         ///< Object to receive notification messages from this worker thread
        const AlignmentParameters_t& params ///< Copied internally and not accessed later
    )
    : wxThread(wxTHREAD_JOINABLE),
    m_Parent(parent),
    m_ProcessingCompleted(false),
    m_ThreadAborted(false),
    m_Parameters(params)
    { }

    ExitCode Entry() override;

    /// Signals the thread to finish processing ASAP
    void AbortProcessing();

    ~c_ImageAlignmentWorkerThread() override;
};

#endif // IMPPG_IMAGE_ALIGNMENT_THREAD_HEADER
