/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2021 Filip Szczerek <ga.software@yahoo.com>

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
    CPU & bitmaps processing back end implementation.
*/

#include "common/common.h"
#include "common/imppg_assert.h"
#include "cpu_bmp_proc.h"
#include "cpu_bmp/message_ids.h"
#include "logging/logging.h"
#include "math_utils/convolution.h"
#include "w_lrdeconv.h"
#include "w_tcurve.h"
#include "w_unshmask.h"

namespace imppg::backend {

c_Image CreateBlurredMonoImage(const c_Image& source)
{
    IMPPG_ASSERT(source.GetPixelFormat() == PixelFormat::PIX_MONO32F);

    const unsigned width = source.GetWidth();
    const unsigned height = source.GetHeight();
    auto blurred = c_Image(width, height, source.GetPixelFormat());

    ConvolveSeparable(
        c_PaddedArrayPtr(source.GetRowAs<const float>(0), width, height, source.GetBuffer().GetBytesPerRow()),
        c_PaddedArrayPtr(blurred.GetRowAs<float>(0), width, height, blurred.GetBuffer().GetBytesPerRow()),
        RAW_IMAGE_BLUR_SIGMA_FOR_ADAPTIVE_UNSHARP_MASK
    );

    return blurred;
}

std::unique_ptr<IProcessingBackEnd> CreateCpuBmpProcessingBackend()
{
    return std::make_unique<c_CpuAndBitmapsProcessing>();
}

void c_CpuAndBitmapsProcessing::StartProcessing(c_Image img, ProcessingSettings procSettings)
{
    IMPPG_ASSERT(
        img.GetPixelFormat() == PixelFormat::PIX_MONO32F ||
        img.GetPixelFormat() == PixelFormat::PIX_RGB32F
    );

    m_Img.clear();

    if (img.GetPixelFormat() == PixelFormat::PIX_MONO32F)
    {
        m_Img.emplace_back(std::move(img));
    }
    else if (img.GetPixelFormat() == PixelFormat::PIX_RGB32F)
    {
        auto [r, g, b] = img.SplitRGB();
        m_Img.emplace_back(std::move(r));
        m_Img.emplace_back(std::move(g));
        m_Img.emplace_back(std::move(b));
    }
    else
    {
        IMPPG_ABORT_MSG("unexpected pixel format");
    }

    SetSelection(m_Img.at(0).GetImageRect());

    SetProcessingSettings(procSettings);

    m_UsePreciseToneCurveValues = true;

    ScheduleProcessing(req_type::Sharpening{});
}

void c_CpuAndBitmapsProcessing::SetProcessingCompletedHandler(std::function<void(CompletionStatus)> handler)
{
    m_OnProcessingCompleted = handler;
}

void c_CpuAndBitmapsProcessing::SetProgressTextHandler(std::function<void(wxString)> handler)
{
    m_ProgressTextHandler = handler;
}

const c_Image& c_CpuAndBitmapsProcessing::GetProcessedOutput()
{
    if (m_Worker)
    {
        m_Worker->Wait();
    }
    IMPPG_ASSERT(m_Output.toneCurve.valid);

    if (m_Img.size() == 1)
    {
        return m_Output.toneCurve.img.at(0);
    }
    else
    {
        return m_Output.toneCurve.combined.value();
    }
}

c_CpuAndBitmapsProcessing::c_CpuAndBitmapsProcessing()
{
    m_EvtHandler.Bind(wxEVT_THREAD, &c_CpuAndBitmapsProcessing::OnThreadEvent, this);
}

void c_CpuAndBitmapsProcessing::OnThreadEvent(wxThreadEvent& event)
{
    // On rare occasions it may happen that the event is outdated and had been sent
    // by a previously launched worker thread, which has already deleted itself.
    // In such case, ignore the event.
    if (event.GetInt() != m_CurrentThreadId)
    {
        Log::Print(wxString::Format("Received an outdated event (%s) with threadId = %d\n",
                event.GetId() == ID_PROCESSING_PROGRESS ? "progress" : "completion", event.GetInt()));
        return;
    }

    switch (event.GetId())
    {
        case ID_PROCESSING_PROGRESS:
        {
            Log::Print(wxString::Format("Received a processing progress (%d%%) event from threadId = %d\n",
                    event.GetPayload<WorkerEventPayload>().percentageComplete, event.GetInt()));

            if (m_ProcessingRequest.has_value())
            {
                wxString action;
                std::visit(Overload{
                    [&](const req_type::Sharpening&) { action = _(L"Lucy\u2013Richardson deconvolution"); },
                    [&](const req_type::UnsharpMasking&) { action = _("Unsharp masking"); },
                    [&](const req_type::ToneCurve&) { action = _("Applying tone curve"); },
                }, m_ProcessingRequest.value());

                if (m_ProgressTextHandler)
                {
                    m_ProgressTextHandler(wxString::Format(action + ": %d%%", event.GetPayload<WorkerEventPayload>().percentageComplete));
                }
            }

            break;
        }

    case ID_FINISHED_PROCESSING:
        {
            const WorkerEventPayload &p = event.GetPayload<WorkerEventPayload>();

            Log::Print(wxString::Format("Received a processing completion event from threadId = %d, status = %s\n",
                    event.GetInt(), p.completionStatus == CompletionStatus::COMPLETED ? "COMPLETED" : "ABORTED"));

            OnProcessingStepCompleted(p.completionStatus);

            if (m_ProcessingScheduled)
            {
                Log::Print("Waiting for the worker thread to finish... ");

                if (m_Worker) { m_Worker->Wait(); }

                Log::Print("done\n");

                StartProcessing();
            }
            break;
        }
    }
}

bool c_CpuAndBitmapsProcessing::IsProcessingInProgress()
{
    return m_Worker && m_Worker->IsAlive();
}

void c_CpuAndBitmapsProcessing::ScheduleProcessing(ProcessingRequest request)
{
    if (m_Img.empty()) return;

    // if the previous processing step(s) did not complete, we have to execute it (them) first
    if (std::holds_alternative<req_type::ToneCurve>(request) && !checked_back(m_Output.unsharpMask).valid)
    {
        for (std::size_t i = 0; i < m_Output.unsharpMask.size(); ++i)
        {
            if (!m_Output.unsharpMask[i].valid)
            {
                request = req_type::UnsharpMasking{i};
                break;
            }
        }
    }

    if (const auto* umaskRequest = std::get_if<req_type::UnsharpMasking>(&request))
    {
        if (!m_Output.sharpening.valid)
        {
            request = req_type::Sharpening{};
        }
        else
        {
            for (std::size_t i = 0; i < umaskRequest->maskIdx; ++i)
            {
                if (!m_Output.unsharpMask.at(i).valid)
                {
                    request = req_type::UnsharpMasking{i};
                    break;
                }
            }
        }
    }

    m_ProcessingRequest = request;

    if (!IsProcessingInProgress())
        StartProcessing();
    else
    {
        // Signal the worker thread to finish ASAP.
        if (m_Worker) { m_Worker->Delete(); }

        // Set a flag so that we immediately restart the worker thread
        // after receiving the "processing finished" message.
        m_ProcessingScheduled = true;
    }
}

void c_CpuAndBitmapsProcessing::OnProcessingStepCompleted(CompletionStatus status)
{
    if (m_ProgressTextHandler)
    {
        m_ProgressTextHandler(_("Idle"));
    }

    if (status == CompletionStatus::COMPLETED)
    {
        Log::Print("Processing step completed\n");

        std::visit(Overload{
            [&](const req_type::Sharpening&)
            {
                m_Output.sharpening.valid = true;
                ScheduleProcessing(req_type::UnsharpMasking{0});
            },

            [&](const req_type::UnsharpMasking& umaskRequest)
            {
                m_Output.unsharpMask.at(umaskRequest.maskIdx).valid = true;
                if (umaskRequest.maskIdx < m_Output.unsharpMask.size() - 1)
                {
                    ScheduleProcessing(req_type::UnsharpMasking{umaskRequest.maskIdx + 1});
                }
                else
                {
                    ScheduleProcessing(req_type::ToneCurve{});
                }
            },

            [&](const req_type::ToneCurve&)
            {
                m_Output.toneCurve.valid = true;

                if (m_Img.size() == 3)
                {
                    m_Output.toneCurve.combined = c_Image::CombineRGB(
                        m_Output.toneCurve.img.at(0),
                        m_Output.toneCurve.img.at(1),
                        m_Output.toneCurve.img.at(2)
                    );
                }

                if (m_OnProcessingCompleted)
                {
                    m_OnProcessingCompleted(status);
                }
            }
        }, m_ProcessingRequest.value());
    }
    else if (status == CompletionStatus::ABORTED && m_OnProcessingCompleted)
    {
        m_OnProcessingCompleted(status);
    }
}

void c_CpuAndBitmapsProcessing::StartLRDeconvolution()
{
    auto& img = m_Output.sharpening.img;
    if (img.empty() ||
        static_cast<int>(img.at(0).GetWidth()) != m_Selection.width ||
        static_cast<int>(img.at(0).GetHeight()) != m_Selection.height)
    {
        img.clear();
        for (std::size_t i = 0; i < m_Img.size(); ++i)
        {
            img.emplace_back(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
        }
    }

    // invalidate the current output and those of subsequent steps
    m_Output.sharpening.valid = false;
    for (auto& umres: m_Output.unsharpMask) { umres.valid = false; }
    m_Output.toneCurve.valid = false;

    if (m_ProcSettings.LucyRichardson.iterations == 0)
    {
        Log::Print("Sharpening disabled, no work needed\n");

        // No processing required, just copy the selection into `output.sharpening.img`,
        // as it will be used by the subsequent processing steps.

        for (std::size_t i = 0; i < m_Img.size(); ++i)
        {
            c_Image::Copy(
                m_Img.at(i),
                m_Output.sharpening.img.at(i),
                m_Selection.x,
                m_Selection.y,
                m_Selection.width,
                m_Selection.height,
                0,
                0
            );
        }
        OnProcessingStepCompleted(CompletionStatus::COMPLETED);
    }
    else
    {
        Log::Print(wxString::Format("Launching L-R deconvolution worker thread (id = %d)\n",
                m_CurrentThreadId));

        // sharpening thread takes the currently selected fragment of the original image as input

        std::vector<c_View<const IImageBuffer>> input;
        std::vector<c_View<IImageBuffer>> output;
        for (std::size_t ch = 0; ch < m_Img.size(); ++ch)
        {
            input.emplace_back(m_Img.at(ch).GetBuffer(), m_Selection.x, m_Selection.y, m_Selection.width, m_Selection.height);
            output.emplace_back(m_Output.sharpening.img.at(ch).GetBuffer());
        }

        m_Worker = std::make_unique<c_LucyRichardsonThread>(
            WorkerParameters{
                m_EvtHandler,
                0, // in the future we will pass the index of the currently open image
                std::move(input),
                std::move(output),
                m_CurrentThreadId
            },
            m_ProcSettings.LucyRichardson.sigma,
            m_ProcSettings.LucyRichardson.iterations,
            m_ProcSettings.LucyRichardson.deringing.enabled,
            DERINGING_BRIGHTNESS_THRESHOLD, m_ProcSettings.LucyRichardson.sigma,
            m_DeringingWorkBuf
        );

        if (m_ProgressTextHandler)
        {
            m_ProgressTextHandler(wxString::Format(_(L"L\u2013R deconvolution") + ": %d%%", 0));
        }

        m_Worker->Run();
    }
}

void c_CpuAndBitmapsProcessing::StartUnsharpMasking(std::size_t maskIdx)
{
    auto& img = m_Output.unsharpMask.at(maskIdx).img;
    if (img.empty() ||
        static_cast<int>(img.at(0).GetWidth()) != m_Selection.width ||
        static_cast<int>(img.at(0).GetHeight()) != m_Selection.height)
    {
        // create a new empty image to hold the unsharp masking result
        img.clear();
        for (std::size_t ch = 0; ch < m_Img.size(); ++ch)
        {
            img.emplace_back(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
        }
    }

    // invalidate the current output and those of subsequent steps
    for (std::size_t i = maskIdx; i < m_Output.unsharpMask.size(); ++i)
    {
        m_Output.unsharpMask.at(i).valid = false;
    }
    m_Output.toneCurve.valid = false;

    const auto& prevStepOutput = [&]() -> auto& {
        if (0 == maskIdx)
        {
            return m_Output.sharpening.img;
        }
        else
        {
            return m_Output.unsharpMask.at(maskIdx - 1).img;
        }
    }();

    if (!m_ProcSettings.unsharpMask.at(maskIdx).IsEffective())
    {
        // no processing required, just take the previous step's output
        const auto numChannels = m_Img.size();
        for (std::size_t ch = 0; ch < numChannels; ++ch)
        {
            c_Image::Copy(
                prevStepOutput.at(ch),
                m_Output.unsharpMask.at(maskIdx).img.at(ch),
                0, 0, m_Selection.width, m_Selection.height, 0, 0
            );
        }
        OnProcessingStepCompleted(CompletionStatus::COMPLETED);
    }
    else
    {
        Log::Print(wxString::Format("Launching unsharp masking worker thread (id = %d)\n", m_CurrentThreadId));

        std::vector<c_View<const IImageBuffer>> input;
        std::vector<c_View<IImageBuffer>> output;
        for (std::size_t ch = 0; ch < m_Img.size(); ++ch)
        {
            input.emplace_back(prevStepOutput.at(ch).GetBuffer());
            output.emplace_back(m_Output.unsharpMask.at(maskIdx).img.at(ch).GetBuffer());
        }

        auto blurred = m_ImgMonoBlurred.has_value()
            ? std::make_optional(c_View<const IImageBuffer>(m_ImgMonoBlurred.value().GetBuffer(), m_Selection))
            : std::nullopt;

        m_Worker = std::make_unique<c_UnsharpMaskingThread>(
            WorkerParameters{
                m_EvtHandler,
                0,
                std::move(input),
                std::move(output),
                m_CurrentThreadId
            },
            std::move(blurred),
            m_ProcSettings.unsharpMask.at(maskIdx)
        );

        if (m_ProgressTextHandler)
        {
            m_ProgressTextHandler(wxString(_("Unsharp masking...")));
        }

        m_Worker->Run();
    }
}

void c_CpuAndBitmapsProcessing::StartToneCurve()
{
    auto& img = m_Output.toneCurve.img;
    if (img.empty() ||
        static_cast<int>(img.at(0).GetWidth()) != m_Selection.width ||
        static_cast<int>(img.at(0).GetHeight()) != m_Selection.height)
    {
        img.clear();
        for (std::size_t i = 0; i < m_Img.size(); ++i)
        {
            img.emplace_back(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
        }
    }

    Log::Print("Created tone curve output image\n");

    // Invalidate the current output
    m_Output.toneCurve.valid = false;

    if (m_ProcSettings.toneCurve.IsIdentity())
    {
        Log::Print("Tone curve is an identity map, no work needed\n");

        for (std::size_t ch = 0; ch < m_Img.size(); ++ch)
        {
            c_Image::Copy(
                checked_back(m_Output.unsharpMask).img.at(ch),
                m_Output.toneCurve.img.at(ch),
                0,
                0,
                m_Selection.width,
                m_Selection.height,
                0,
                0
            );
        }

        OnProcessingStepCompleted(CompletionStatus::COMPLETED);
    }
    else
    {
        Log::Print(wxString::Format("Launching tone curve worker thread (id = %d)\n",
                m_CurrentThreadId));

        // tone curve thread takes the output of unsharp masking as input

        std::vector<c_View<const IImageBuffer>> input;
        std::vector<c_View<IImageBuffer>> output;
        for (std::size_t ch = 0; ch < m_Img.size(); ++ch)
        {
            input.emplace_back(checked_back(m_Output.unsharpMask).img.at(ch).GetBuffer());
            output.emplace_back(m_Output.toneCurve.img.at(ch).GetBuffer());
        }

        m_Worker = std::make_unique<c_ToneCurveThread>(
            WorkerParameters{
                m_EvtHandler,
                0, // in the future we will pass the index of currently open image
                std::move(input),
                std::move(output),
                m_CurrentThreadId
            },
            m_ProcSettings.toneCurve,
            m_UsePreciseToneCurveValues
        );

        if (m_ProgressTextHandler)
        {
            m_ProgressTextHandler(wxString::Format(_("Applying tone curve: %d%%"), 0));
        }

        m_Worker->Run();
    }
}

void c_CpuAndBitmapsProcessing::StartProcessing()
{
    IMPPG_ASSERT(!m_Img.empty());

    Log::Print("Starting processing\n");

    // sanity check; the background thread should be finished and deleted at this point
    if (IsProcessingInProgress())
    {
        Log::Print("WARNING: The worker thread is still running!\n");
        return;
    }

    m_ProcessingScheduled = false;

    // Make sure that if there are outdated thread events out there, they will be recognized
    // as such and discarded (`currentThreadId` will be sent from worker in event.GetInt()).
    // See also: OnThreadEvent().
    m_CurrentThreadId += 1;

    std::visit(Overload{
        [&](const req_type::Sharpening&) { StartLRDeconvolution(); },

        [&](const req_type::UnsharpMasking& umaskRequest) { StartUnsharpMasking(umaskRequest.maskIdx); },

        [&](const req_type::ToneCurve&) { StartToneCurve(); }
    }, m_ProcessingRequest.value());
}

c_CpuAndBitmapsProcessing::~c_CpuAndBitmapsProcessing()
{
    if (m_Worker)
    {
        m_Worker->Delete();
        m_Worker->Wait();
    }
}

void c_CpuAndBitmapsProcessing::AbortProcessing()
{
    if (m_Worker)
    {
        Log::Print("Sending abort request to the worker thread\n");
        m_Worker->Delete();
        m_Worker->Wait();
    }
}

void c_CpuAndBitmapsProcessing::ApplyPreciseToneCurveValues()
{
    IMPPG_ASSERT(!IsProcessingInProgress());

    if (m_Output.toneCurve.preciseValuesApplied)
    {
        return;
    }

    if (!checked_back(m_Output.unsharpMask).valid)
    {
        for (auto& umOutput: m_Output.unsharpMask)
        {
            umOutput.img.clear();
            for (std::size_t i = 0; i < m_Img.size(); ++i)
            {
                umOutput.img.emplace_back(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
            }

            const auto numChannels = m_Img.size();
            for (std::size_t ch = 0; ch < numChannels; ++ch)
            {
                c_Image::Copy(
                    m_Img.at(ch),
                    umOutput.img.at(ch),
                    m_Selection.x,
                    m_Selection.y,
                    m_Selection.width,
                    m_Selection.height,
                    0,
                    0
                );
            }
        }
    }

    if (!m_Output.toneCurve.valid)
    {
        m_Output.toneCurve.img.clear();
        for (std::size_t i = 0; i < m_Img.size(); ++i)
        {
            m_Output.toneCurve.img.emplace_back(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
        }

        for (std::size_t i = 0; i < m_Img.size(); ++i)
        {
            c_Image::Copy(
                checked_back(m_Output.unsharpMask).img.at(i),
                m_Output.toneCurve.img.at(i),
                0,
                0,
                m_Selection.width,
                m_Selection.height,
                0,
                0
            );
        }
    }

    IMPPG_ASSERT(checked_back(m_Output.unsharpMask).img.at(0).GetImageRect() == m_Output.toneCurve.img.at(0).GetImageRect());

    c_Image& src = checked_back(m_Output.unsharpMask).img.at(0);
    c_Image& dest = m_Output.toneCurve.img.at(0);
    for (unsigned y = 0; y < src.GetHeight(); ++y)
    {
        m_ProcSettings.toneCurve.ApplyPreciseToneCurve(
            src.GetRowAs<float>(y),
            dest.GetRowAs<float>(y),
            src.GetWidth()
        );
    }

    m_Output.toneCurve.preciseValuesApplied = true;
}

void c_CpuAndBitmapsProcessing::SetSelection(wxRect selection)
{
    m_Selection = selection;
    m_DeringingWorkBuf.resize(selection.width * selection.height);
}

void c_CpuAndBitmapsProcessing::SetImage(c_Image img)
{
    IMPPG_ASSERT(
        img.GetPixelFormat() == PixelFormat::PIX_MONO32F ||
        img.GetPixelFormat() == PixelFormat::PIX_RGB32F
    );

    m_Img.clear();

    if (img.GetPixelFormat() == PixelFormat::PIX_MONO32F)
    {
        if (m_ProcSettings.unsharpMask.at(0).adaptive)
        {
            m_ImgMonoBlurred = CreateBlurredMonoImage(img);
        }
        m_Img.emplace_back(std::move(img));
    }
    else
    {
        if (m_ProcSettings.unsharpMask.at(0).adaptive)
        {
            const auto mono = img.ConvertPixelFormat(PixelFormat::PIX_MONO32F);
            m_ImgMonoBlurred = CreateBlurredMonoImage(mono);
        }

        auto [r, g, b] = img.SplitRGB();
        m_Img.emplace_back(std::move(r));
        m_Img.emplace_back(std::move(g));
        m_Img.emplace_back(std::move(b));
    }
}

std::optional<const std::vector<c_Image>*> c_CpuAndBitmapsProcessing::GetUnshMaskOutput() const
{
    const auto& umResult = checked_back(m_Output.unsharpMask);
    if (!umResult.img.empty() && umResult.valid)
    {
        return &umResult.img;
    }
    else
    {
        return std::nullopt;
    }
}

void c_CpuAndBitmapsProcessing::SetProcessingSettings(ProcessingSettings procSettings)
{
    IMPPG_ASSERT(procSettings.unsharpMask.size() >= 1);

    const bool adaptiveUnshMaskSwitchedOn =
        procSettings.AdaptiveUnshMaskEnabled() && ! m_ProcSettings.AdaptiveUnshMaskEnabled();

    const bool unshMaskCountChanged = (procSettings.unsharpMask.size() != m_ProcSettings.unsharpMask.size());

    m_ProcSettings = std::move(procSettings);

    if (adaptiveUnshMaskSwitchedOn && !m_Img.empty())
    {
        if (m_Img.at(0).GetPixelFormat() == PixelFormat::PIX_MONO32F)
        {
            m_ImgMonoBlurred = CreateBlurredMonoImage(m_Img.at(0));
        }
        else
        {
            const auto mono = c_Image::CombineRGB(m_Img.at(0), m_Img.at(1), m_Img.at(2));
            m_ImgMonoBlurred = CreateBlurredMonoImage(mono);
        }
    }

    if (unshMaskCountChanged)
    {
        m_Output.unsharpMask = std::vector(m_ProcSettings.unsharpMask.size(), UnsharpMaskResult{});
    }
}

} // namespace imppg::backend
