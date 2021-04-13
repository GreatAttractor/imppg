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

#include "backend/cpu_bmp/cpu_bmp_proc.h"
#include "backend/cpu_bmp/w_lrdeconv.h"
#include "backend/cpu_bmp/w_tcurve.h"
#include "backend/cpu_bmp/w_unshmask.h"
#include "common.h"
#include "ctrl_ids.h"
#include "imppg_assert.h"
#include "logging.h"

namespace imppg::backend {

void c_CpuAndBitmapsProcessing::StartProcessing(c_Image img, ProcessingSettings procSettings)
{
    m_OwnedImg = std::move(img);
    m_Img = &m_OwnedImg.value();
    SetSelection(m_OwnedImg.value().GetImageRect());
    m_ProcSettings = procSettings;
    m_UsePreciseToneCurveValues = true;

    ScheduleProcessing(ProcessingRequest::SHARPENING);
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

    return m_Output.toneCurve.img.value();
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

            wxString action;
            if (m_ProcessingRequest == ProcessingRequest::SHARPENING)
                action = _(L"Lucy\u2013Richardson deconvolution");
            else if (m_ProcessingRequest == ProcessingRequest::UNSHARP_MASKING)
                action = _("Unsharp masking");
            else if (m_ProcessingRequest == ProcessingRequest::TONE_CURVE)
                action = _("Applying tone curve");

            if (m_ProgressTextHandler)
            {
                m_ProgressTextHandler(std::move(wxString::Format(action + ": %d%%", event.GetPayload<WorkerEventPayload>().percentageComplete)));
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

                // Since we have just received the "finished processing" event, the worker thread will destroy itself any moment; keep polling
                while (IsProcessingInProgress())
                    wxThread::Yield();

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
    if (!m_Img) return;

    ProcessingRequest originalReq = request;

    // if the previous processing step(s) did not complete, we have to execute it (them) first
    if (request == ProcessingRequest::TONE_CURVE && !m_Output.unsharpMasking.valid)
        request = ProcessingRequest::UNSHARP_MASKING;

    if (request == ProcessingRequest::UNSHARP_MASKING && !m_Output.sharpening.valid)
        request = ProcessingRequest::SHARPENING;

    Log::Print(wxString::Format("Scheduling processing; requested: %d, scheduled: %d\n",
        static_cast<int>(originalReq),
        static_cast<int>(request))
    );

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
    m_ProcRequestInProgress = ProcessingRequest::NONE;

    if (m_ProgressTextHandler)
    {
        m_ProgressTextHandler(std::move(_("Idle")));
    }

    if (status == CompletionStatus::COMPLETED)
    {
        Log::Print("Processing step completed\n");

        if (m_ProcessingRequest == ProcessingRequest::SHARPENING)
        {
            m_Output.sharpening.valid = true;
            ScheduleProcessing(ProcessingRequest::UNSHARP_MASKING);
        }
        else if (m_ProcessingRequest == ProcessingRequest::UNSHARP_MASKING)
        {
            m_Output.unsharpMasking.valid = true;
            ScheduleProcessing(ProcessingRequest::TONE_CURVE);
        }
        else if (m_ProcessingRequest == ProcessingRequest::TONE_CURVE)
        {
            m_Output.toneCurve.valid = true;

            if (m_OnProcessingCompleted)
            {
                m_OnProcessingCompleted(status);
            }
        }
    }
    else if (status == CompletionStatus::ABORTED && m_OnProcessingCompleted)
    {
        m_OnProcessingCompleted(status);
    }
}

void c_CpuAndBitmapsProcessing::StartLRDeconvolution()
{
    auto& img = m_Output.sharpening.img;
    if (!img.has_value() ||
        static_cast<int>(img->GetWidth()) != m_Selection.width ||
        static_cast<int>(img->GetHeight()) != m_Selection.height)
    {
        img = c_Image(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
    }

    // invalidate the current output and those of subsequent steps
    m_Output.sharpening.valid = false;
    m_Output.unsharpMasking.valid = false;
    m_Output.toneCurve.valid = false;

    if (m_ProcSettings.LucyRichardson.iterations == 0)
    {
        Log::Print("Sharpening disabled, no work needed\n");

        // No processing required, just copy the selection into `output.sharpening.img`,
        // as it will be used by the subsequent processing steps.

        c_Image::Copy(*m_Img, m_Output.sharpening.img.value(),
            m_Selection.x,
            m_Selection.y,
            m_Selection.width,
            m_Selection.height,
            0, 0);
        OnProcessingStepCompleted(CompletionStatus::COMPLETED);
    }
    else
    {
        Log::Print(wxString::Format("Launching L-R deconvolution worker thread (id = %d)\n",
                m_CurrentThreadId));

        // Sharpening thread takes the currently selected fragment of the original image as input
        m_ProcRequestInProgress = ProcessingRequest::SHARPENING;
        m_Worker = std::make_unique<c_LucyRichardsonThread>(
            WorkerParameters{
                m_EvtHandler,
                0, // in the future we will pass the index of the currently open image
                c_View<const IImageBuffer>(
                    m_Img->GetBuffer(),
                    m_Selection.x,
                    m_Selection.y,
                    m_Selection.width,
                    m_Selection.height
                ),
                m_Output.sharpening.img.value().GetBuffer(),
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
            m_ProgressTextHandler(std::move(wxString::Format(_(L"L\u2013R deconvolution") + ": %d%%", 0)));
        }

        m_Worker->Run();
    }
}

void c_CpuAndBitmapsProcessing::StartUnsharpMasking()
{
    auto& img = m_Output.unsharpMasking.img;
    if (!img.has_value() ||
        static_cast<int>(img->GetWidth()) != m_Selection.width ||
        static_cast<int>(img->GetHeight()) != m_Selection.height)
    {
        img = c_Image(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
    }

    // invalidate the current output and those of subsequent steps
    m_Output.unsharpMasking.valid = false;
    m_Output.toneCurve.valid = false;

    if (!m_ProcSettings.unsharpMasking.IsEffective())
    {
        Log::Print("Unsharp masking disabled, no work needed\n");

        // No processing required, just copy the selection into `output.sharpening.img`,
        // as it will be used by the subsequent processing steps.
        c_Image::Copy(m_Output.sharpening.img.value(), m_Output.unsharpMasking.img.value(), 0, 0, m_Selection.width, m_Selection.height, 0, 0);
        OnProcessingStepCompleted(CompletionStatus::COMPLETED);
    }
    else
    {
        Log::Print(wxString::Format("Launching unsharp masking worker thread (id = %d)\n",
            m_CurrentThreadId));

        // unsharp masking thread takes the output of sharpening as input
        m_ProcRequestInProgress = ProcessingRequest::UNSHARP_MASKING;
        m_Worker = std::make_unique<c_UnsharpMaskingThread>(
            WorkerParameters{
                m_EvtHandler,
                0, // in the future we will pass the index of currently open image
                m_Output.sharpening.img.value().GetBuffer(),
                m_Output.unsharpMasking.img.value().GetBuffer(),
                m_CurrentThreadId
            },
            c_View<const IImageBuffer>(m_Img->GetBuffer(), m_Selection),
            m_ProcSettings.unsharpMasking.adaptive,
            m_ProcSettings.unsharpMasking.sigma,
            m_ProcSettings.unsharpMasking.amountMin,
            m_ProcSettings.unsharpMasking.amountMax,
            m_ProcSettings.unsharpMasking.threshold,
            m_ProcSettings.unsharpMasking.width
        );

        if (m_ProgressTextHandler)
        {
            m_ProgressTextHandler(std::move(wxString(_("Unsharp masking..."))));
        }

        m_Worker->Run();
    }
}

void c_CpuAndBitmapsProcessing::StartToneCurve()
{
    auto& img = m_Output.toneCurve.img;
    if (!img.has_value() ||
        static_cast<int>(img->GetWidth()) != m_Selection.width ||
        static_cast<int>(img->GetHeight()) != m_Selection.height)
    {
        img = c_Image(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
    }

    Log::Print("Created tone curve output image\n");

    // Invalidate the current output
    m_Output.toneCurve.valid = false;

    if (m_ProcSettings.toneCurve.IsIdentity())
    {
        Log::Print("Tone curve is an identity map, no work needed\n");

        c_Image::Copy(
            m_Output.unsharpMasking.img.value(),
            m_Output.toneCurve.img.value(),
            0,
            0,
            m_Selection.width,
            m_Selection.height,
            0,
            0
        );

        OnProcessingStepCompleted(CompletionStatus::COMPLETED);
    }
    else
    {
        Log::Print(wxString::Format("Launching tone curve worker thread (id = %d)\n",
                m_CurrentThreadId));

        // tone curve thread takes the output of unsharp masking as input
        m_ProcRequestInProgress = ProcessingRequest::TONE_CURVE;
        m_Worker = std::make_unique<c_ToneCurveThread>(
            WorkerParameters{
                m_EvtHandler,
                0, // in the future we will pass the index of currently open image
                m_Output.unsharpMasking.img.value().GetBuffer(),
                m_Output.toneCurve.img.value().GetBuffer(),
                m_CurrentThreadId
            },
            m_ProcSettings.toneCurve,
            m_UsePreciseToneCurveValues
        );

        if (m_ProgressTextHandler)
        {
            m_ProgressTextHandler(std::move(wxString::Format(_("Applying tone curve: %d%%"), 0)));
        }

        m_Worker->Run();
    }
}

void c_CpuAndBitmapsProcessing::StartProcessing()
{
    IMPPG_ASSERT(m_Img);

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

    switch (m_ProcessingRequest)
    {
    case ProcessingRequest::SHARPENING:
        StartLRDeconvolution();
        break;

    case ProcessingRequest::UNSHARP_MASKING:
        StartUnsharpMasking();
        break;

    case ProcessingRequest::TONE_CURVE:
        StartToneCurve();
        break;

        case ProcessingRequest::NONE: IMPPG_ABORT();
    }
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

    if (!m_Output.unsharpMasking.valid)
    {
        m_Output.unsharpMasking.img = c_Image(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
        c_Image::Copy(
            *m_Img,
            m_Output.unsharpMasking.img.value(),
            m_Selection.x,
            m_Selection.y,
            m_Selection.width,
            m_Selection.height,
            0,
            0
        );
    }

    if (!m_Output.toneCurve.valid)
    {
        m_Output.toneCurve.img = c_Image(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
        c_Image::Copy(
            m_Output.unsharpMasking.img.value(),
            m_Output.toneCurve.img.value(),
            0,
            0,
            m_Selection.width,
            m_Selection.height,
            0,
            0
        );
    }

    IMPPG_ASSERT(m_Output.unsharpMasking.img.value().GetImageRect() == m_Output.toneCurve.img.value().GetImageRect());

    c_Image& src = m_Output.unsharpMasking.img.value();
    c_Image& dest = m_Output.toneCurve.img.value();
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

} // namespace imppg::backend
