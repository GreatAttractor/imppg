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
    Back end interface declaration.
*/

#ifndef IMPPG_BACKEND_HEADER
#define IMPPG_BACKEND_HEADER

#include <functional>
#include <optional>
#include <wx/scrolwin.h>

#include "common.h"
#include "image.h"
#include "proc_settings.h"

namespace imppg::backend {

enum class CompletionStatus
{
    COMPLETED = 0,
    ABORTED
};

class IBackEnd
{
public:
    virtual void ImageViewScrolledOrResized(float zoomFactor) = 0;

    virtual void ImageViewZoomChanged(float zoomFactor) = 0;

    virtual void SetImage(c_Image&& img, std::optional<wxRect> newSelection) = 0;

    /// Shall start processing of the selected image fragment immediately.
    virtual void NewSelection(
        const wxRect& selection ///< New selection for processing, in logical image coords.
    ) = 0;

    virtual void SetProcessingCompletedHandler(std::function<void(CompletionStatus)>) = 0;

    /// Provides getter of selection in physical image view coords.
    ///
    /// To be used for marking the selection on screen.
    ///
    virtual void SetPhysicalSelectionGetter(std::function<wxRect()> getter) = 0;

    virtual void SetScaledLogicalSelectionGetter(std::function<wxRect()> getter) = 0;

    /// Returns histogram of current selection after processing, but before applying tone curve.
    virtual Histogram GetHistogram() = 0;

    /// Called after the main window is first shown with this back end instance; returns `false`
    /// if back end initialization failed.
    virtual bool MainWindowShown() { return true; }

    /// Invalidates (marks to be repainted) a rectangle in the image view.
    ///
    /// The back end may choose to repaint the whole image view instead.
    ///
    virtual void RefreshRect(const wxRect& rect) = 0;

    virtual void NewProcessingSettings(const ProcessingSettings& procSettings) = 0;

    virtual void LRSettingsChanged(const ProcessingSettings& procSettings) = 0;

    virtual void UnshMaskSettingsChanged(const ProcessingSettings& procSettings) = 0;

    virtual void ToneCurveChanged(const ProcessingSettings& procSettings) = 0;

    virtual void SetScalingMethod(ScalingMethod scalingMethod) = 0;

    /// Returns the original image being edited.
    virtual const std::optional<c_Image>& GetImage() const = 0;

    /// Provides a function to be called when progress text of back end's operations changes.
    virtual void SetProgressTextHandler(std::function<void(wxString)>) {}

    /// Shall be called by the main window from "on idle" handler; the back end may call `event.RequestMore()`.
    virtual void OnIdle(wxIdleEvent&) {}

    /// Returns processed contents of current selection.
    ///
    /// If processing is in progress, aborts it and returns the most recent processing results (if any)
    /// or just the unprocessed selection of the input image.
    ///
    /// NOTE: if current selection has different position than the previously processed selection,
    /// but has the same dimensions, the existing processing result is used anyway. If needed, the caller
    /// should prevent it on its own.
    ///
    virtual c_Image GetProcessedSelection() = 0;

    virtual bool ProcessingInProgress() = 0;

    virtual void AbortProcessing() = 0;

    virtual ~IBackEnd() = default;
};

} // namespace imppg

#endif // IMPPG_BACKEND_HEADER
