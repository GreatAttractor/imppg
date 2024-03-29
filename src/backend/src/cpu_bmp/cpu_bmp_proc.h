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
    CPU & bitmaps processing back end declaration.
*/

#ifndef IMPPG_CPU_BMP_PROC_HEADER
#define IMPPG_CPU_BMP_PROC_HEADER

#include "backend/backend.h"
#include "cpu_bmp/worker.h"

#include <functional>
#include <optional>
#include <vector>

namespace imppg::backend {

class c_CpuAndBitmapsProcessing: public IProcessingBackEnd
{
public:

    // IProcessingBackEnd functions ---------------------------------------------------------------

    void StartProcessing(c_Image img, ProcessingSettings procSettings) override;

    void SetProcessingCompletedHandler(std::function<void(CompletionStatus)> handler) override;

    void SetProgressTextHandler(std::function<void(wxString)> handler) override;

    const c_Image& GetProcessedOutput() override;

    void AbortProcessing() override;

    // --------------------------------------------------------------------------------------------

    c_CpuAndBitmapsProcessing();

    c_CpuAndBitmapsProcessing(const c_CpuAndBitmapsProcessing&) = delete;

    c_CpuAndBitmapsProcessing& operator=(const c_CpuAndBitmapsProcessing&) = delete;

    c_CpuAndBitmapsProcessing(c_CpuAndBitmapsProcessing&&) = delete;

    c_CpuAndBitmapsProcessing& operator=(c_CpuAndBitmapsProcessing&&) = delete;

    ~c_CpuAndBitmapsProcessing() override;

    void SetImage(c_Image img);

    void SetSelection(wxRect selection);

    void SetProcessingSettings(ProcessingSettings procSettings);

    /// Aborts processing and schedules new processing to start ASAP (as soon as worker thread is not running).
    void ScheduleProcessing(ProcessingRequest request);

    /// Returns unsharp masking result if it is valid.
    std::optional<const std::vector<c_Image>*> GetUnshMaskOutput() const;

    /// Applies precise tone curve values to unsharp masking output if it is valid; otherwise,
    /// applies them to (fragment of) original image.
    void ApplyPreciseToneCurveValues();

    /// Returns `true` if the processing thread is running.
    bool IsProcessingInProgress();

private:

    /// Creates and starts a background processing thread.
    void StartProcessing();

    void StartLRDeconvolution();

    void StartUnsharpMasking(std::size_t maskIdx);

    void StartToneCurve();

    void OnProcessingStepCompleted(CompletionStatus status);

    void OnThreadEvent(wxThreadEvent& event);

    /// Image being processed; if not empty, contains 1 element (mono luminance) or 3 (R, G, B channels).
    std::vector<c_Image> m_Img;

    /// Mono version of `m_Img` used for adaptive unsharp masking.
    std::optional<c_Image> m_ImgMonoBlurred;

    wxRect m_Selection; ///< Fragment of `m_Img` selected for processing (in logical image coords).

    wxEvtHandler m_EvtHandler;

    std::function<void(wxString)> m_ProgressTextHandler;

    ProcessingSettings m_ProcSettings{};

    std::vector<uint8_t> m_DeringingWorkBuf;

    std::unique_ptr<IWorkerThread> m_Worker;

    /// Identifier increased by 1 after each creation of a new thread
    int m_CurrentThreadId{0};

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
    std::optional<ProcessingRequest> m_ProcessingRequest;

    /// If `true`, processing has been scheduled to start ASAP (as soon as `m_Processing.worker` is not running)
    bool m_ProcessingScheduled{false};

    struct UnsharpMaskResult
    {
        std::vector<c_Image> img; ///< 1 or 3 elements: luminance or R, G, B channels.
        bool valid{false}; ///< `true` if the last unsharp masking request completed.
    };

    /// Incremental results of processing of the current selection.
    /** Must not be accessed when the relevant background thread is running. */
    struct
    {
        /// Results of sharpening.
        struct
        {
            std::vector<c_Image> img; ///< 1 or 3 elements: luminance or R, G, B channels.
            bool valid{false}; ///< `true` if the last sharpening request completed.
        } sharpening;

        /// Results of sharpening and unsharp masking. By convention, there is always at least one element,
        /// even if unsharp masking is a no-op (i.e., amount = 1.0).
        std::vector<UnsharpMaskResult> unsharpMask{UnsharpMaskResult{}};

        /// Results of sharpening, unsharp masking and applying of tone curve.
        struct
        {
            std::vector<c_Image> img; ///< 1 or 3 elements: luminance or R, G, B channels.
            std::optional<c_Image> combined; ///< Luminance or combined R, G, B channels.
            bool valid{false}; ///< `true` if the last tone curve application request completed.
            bool preciseValuesApplied{false}; ///< 'true' if precise values of tone curve have been applied; happens only when saving output file.
        } toneCurve;
    } m_Output;

    std::function<void(CompletionStatus)> m_OnProcessingCompleted;

    bool m_UsePreciseToneCurveValues{false};
};

}  // namespace imppg::backend

#endif // IMPPG_CPU_BMP_PROC_HEADER
