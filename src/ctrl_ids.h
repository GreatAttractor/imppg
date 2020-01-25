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
    Ids of wxWidgets controls.
*/

#ifndef IMPPG_CTRL_IDS_HEADER
#define IMPPG_CTRL_IDS_HEADER


#include "appconfig.h"

// Control IDs must be unique to avoid problems with FindWindowById(); since we are using wxAui,
// a control (containing children) may get undocked, becoming one of top-level windows and
// changing the order FindWindowById() performs its search. If there were several control series
// with values starting at wxID_HIGHEST, we could get an unexpected result.
enum
{

//----------------------------------------------------------------------
// Main window and processing settings panel

    ID_AllBegin = wxID_HIGHEST + 1,

    ID_ImageView,

    ID_ProcessingControlsPanel,

    ID_LucyRichardsonIters,
    ID_LucyRichardsonSigma,
    ID_LucyRichardsonReset,
    ID_LucyRichardsonDeringing,
    ID_LucyRichardsonOff,

    ID_UnsharpMaskingSigma,
    ID_UnsharpMaskingAmountMin,
    ID_UnsharpMaskingAmountMax,
    ID_UnsharpMaskingReset,
    ID_UnsharpMaskingAdaptive,
    ID_UnsharpMaskingThreshold,
    ID_UnsharpMaskingWidth,

    ID_ToneCurveEditor,

    // File menu items
    ID_FileMenuStart,
    ID_LoadSettings,
    ID_SaveSettings,
    ID_BatchProcessing,
    ID_FileMenuEnd,

    // Settings menu items
    ID_CpuBmpBackEnd,
    ID_OpenGLBackEnd,
    ID_NormalizeImage,
    ID_ChooseLanguage,
    ID_ToolIconSize,
    ID_ToneCurveWindowSettings,
    ID_ToneCurveWindowReset,

    // View menu items
    ID_Panels,
    ID_ToggleProcessingPanel,
    ID_ToggleToneCurveEditor,

    ID_ZoomRangeBegin,
    ID_ZoomIn,
    ID_ZoomOut,
    ID_Zoom33,
    ID_Zoom50,
    ID_Zoom100,
    ID_Zoom150,
    ID_Zoom200,
    ID_ZoomCustom,
    ID_ScalingNearest,
    ID_ScalingLinear,
    ID_ScalingCubic,
    ID_ZoomRangeEnd,

    ID_About,
    ID_SelectAndProcessAll,
    ID_FitInWindow,
    ID_AlignImages,

//----------------------------------------------------------------------
// Normalization dialog

    ID_MinLevel,
    ID_MaxLevel,
    ID_NormalizationEnabled,

//----------------------------------------------------------------------
// Batch processing progress dialog

    ID_Grid,
    ID_ProgressGauge,
    ID_Close,
    ID_ProcessingStepSkipped,

//----------------------------------------------------------------------
// Tone curve editor

    ID_Reset,
    ID_Invert,
    ID_Smooth,
    ID_Logarithmic,
    ID_GammaCheckBox,
    ID_GammaCtrl,
    ID_Stretch,
    ID_UpdateEvtTimer,
    ID_Configure,

//----------------------------------------------------------------------
// "About" dialog
    ID_Libraries,

//----------------------------------------------------------------------
// Background thread messages

    ID_FINISHED_PROCESSING, // used by image processing worker threads
    ID_PROCESSING_PROGRESS, //

//----------------------------------------------------------------------
// Other items
    ID_MruSettings, ///< Dropdown list of the most recently used proc. settings
    ID_MruListItem,
    ID_MruListItemLast = ID_MruListItem + Configuration::MAX_MRU_SETTINGS_ITEMS,
    ID_MruListClear = ID_MruListItemLast,


};

#endif // IMPPG_CTRL_IDS_HEADER
