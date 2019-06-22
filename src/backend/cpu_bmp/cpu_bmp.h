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
    CPU & bitmaps back end declaration.
*/

#ifndef IMPPG_CPU_BMP_HEADER
#define IMPPG_CPU_BMP_HEADER

#include <functional>
#include <optional>
#include <wx/bitmap.h>
#include <wx/event.h>
#include <wx/timer.h>

#include "../../common.h"
#include "../../exclusive_access.h"
#include "../../scrolled_view.h"
#include "../backend.h"
#include "worker.h"

namespace imppg::backend {

class c_CpuAndBitmaps: public IBackEnd
{
public:
    c_CpuAndBitmaps(c_ScrolledView& imgView);

    void ImageViewScrolledOrResized(float zoomFactor) override;

    void ImageViewZoomChanged(float zoomFactor) override;

    void FileOpened(c_Image&& img, std::optional<wxRect> newSelection) override;

    Histogram GetHistogram() override;

    void SetPhysicalSelectionGetter(std::function<wxRect()> getter) override { m_PhysSelectionGetter = getter; }

    void SetScaledLogicalSelectionGetter(std::function<wxRect()> getter) override { m_ScaledLogicalSelectionGetter = getter; }

    void SetProcessingCompletedHandler(std::function<void()> handler) override { m_OnProcessingCompleted = handler; }

    void NewSelection(const wxRect& selection) override;

    void RefreshRect(const wxRect& rect) override;

    void NewProcessingSettings(const ProcessingSettings& procSettings) override;

    void LRSettingsChanged(const ProcessingSettings& procSettings) override;

    void UnshMaskSettingsChanged(const ProcessingSettings& procSettings) override;

    void ToneCurveChanged(const ProcessingSettings& procSettings) override;

private:
    wxEvtHandler m_EvtHandler;

    c_ScrolledView& m_ImgView;

    std::optional<c_Image> m_Img;

    std::optional<wxBitmap> m_ImgBmp; ///< Bitmap which wraps `m_Img` for displaying on `m_ImgView`.

    float m_ZoomFactor{ZOOM_NONE};

    float m_NewZoomFactor{ZOOM_NONE};

    std::optional<wxBitmap> m_BmpScaled; ///< Currently visible scaled fragment (or whole) of `m_ImgBmp`.

    wxRect m_ScaledArea; ///< Area within `m_ImgBmp` represented by `m_BmpScaled`.

    wxRect m_Selection; ///< Image fragment selected for processing (in logical image coords).

    /// Provides selection in physical image view coords.
    std::function<wxRect()> m_PhysSelectionGetter;

    std::function<wxRect()> m_ScaledLogicalSelectionGetter;

    std::function<void()> m_OnProcessingCompleted;

    class c_ScalingTimer: public wxTimer
    {
        std::function<void()> m_Handler;
    public:
        void SetHandler(std::function<void()> handler) { m_Handler = handler; }
        void Notify() override { m_Handler(); }
    } m_ScalingTimer;

    ProcessingSettings m_ProcSettings{};

    /// Background processing-related variables
    struct
    {
        ExclusiveAccessObject<IWorkerThread*> worker{nullptr};

        /// Identifier increased by 1 after each creation of a new thread
        int currentThreadId{0};
        /// If 'true', tone curve is applied using its precise values. Otherwise, the curve's LUT is used.
        /** Set to 'false' when user is just editing and adjusting settings. Set to 'true' when an output file is saved. */
        bool usePreciseTCurveVals{false};

        /// Currently scheduled processing request.
        ///
        /// When a request of type `n` (from the enum below) is generated (by user actions in the GUI),
        /// all processing steps designated by values >=n are performed.
        ///
        /// For example, if the user changes sharpening settings (e.g. the L-R kernel sigma),
        /// the currently selected area is sharpened, then unsharp masking is performed,
        /// and finally the tone curve applied. If, however, the user changes only a control point
        /// in the tone curve editor, only the (updated) tone curve is applied (to the results
        /// of the last performed unsharp masking).
        ProcessingRequest processingRequest{ProcessingRequest::NONE};

        /// If `true`, processing has been scheduled to start ASAP (as soon as `m_Processing.worker` is not running)
        bool processingScheduled{false};

        /// Incremental results of processing of the current selection.
        /** Must not be accessed when the relevant background thread is running. */
        struct
        {
            /// Results of sharpening.
            struct
            {
                std::optional<c_Image> img;
                bool valid{false}; ///< `true` if the last sharpening request completed.
            } sharpening;

            /// Results of sharpening and unsharp masking.
            struct
            {
                std::optional<c_Image> img;
                bool valid{false}; ///< `true` if the last unsharp masking request completed.
            } unsharpMasking;

            /// Results of sharpening, unsharp masking and applying of tone curve.
            struct
            {
                std::optional<c_Image> img;
                bool valid{false}; ///< `true` if the last tone curve application request completed.
                bool preciseValuesApplied{false}; ///< 'true' if precise values of tone curve have been applied; happens only when saving output file.
            } toneCurve;
        } output;

    } m_Processing;

    void OnPaint(wxPaintEvent& event);

    void CreateScaledPreview(float zoomFactor);

    /// Aborts processing and schedules new processing to start ASAP (as soon as `m_Processing.worker` is not running).
    void ScheduleProcessing(ProcessingRequest request);

    /// Returns `true` if the processing thread is running.
    bool IsProcessingInProgress();

    /// Creates and starts a background processing thread.
    void StartProcessing();

    void StartLRDeconvolution();

    void StartUnsharpMasking();

    void StartToneCurve();

    void OnProcessingStepCompleted(CompletionStatus status);

    void OnThreadEvent(wxThreadEvent& event);

    void UpdateSelectionAfterProcessing();
};

#endif // IMPPG_CPU_BMP_HEADER

} // namespace imppg::backend
