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
    CPU & bitmaps back end core implementation.
*/

#include <cfloat>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>

#include "../../ctrl_ids.h"
#include "../../logging.h"
#include "cpu_bmp.h"
#include "w_lrdeconv.h"
#include "w_unshmask.h"
#include "w_tcurve.h"

namespace imppg::backend {

/// Delay after a scroll or resize event before refreshing the display if zoom level <> 100%.
constexpr int IMAGE_SCALING_DELAY_MS = 150;

static wxImageResizeQuality GetResizeQuality(ScalingMethod smethod)
{
    switch (smethod)
    {
    case ScalingMethod::NEAREST: return wxIMAGE_QUALITY_NEAREST;
    case ScalingMethod::LINEAR: return wxIMAGE_QUALITY_BILINEAR;
    case ScalingMethod::CUBIC: return wxIMAGE_QUALITY_BICUBIC;
    default: return wxIMAGE_QUALITY_BICUBIC;
    }
}

/// Marks the selection's outline (using physical coords).
static void MarkSelection(const wxRect& physSelection, wxDC& dc)
{
#ifdef __WXMSW__

    wxRasterOperationMode oldMode = dc.GetLogicalFunction();
    dc.SetLogicalFunction(wxINVERT);
    dc.SetPen(*wxBLACK_PEN);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(physSelection);
    dc.SetLogicalFunction(oldMode);

#else
    // On other platforms, e.g. GTK 3 or OS X (but not GTK 2), logical DC operations are not supported.
    // To be on the safe side, draw the selection using a dashed pen instead.

    dc.SetBrush(*wxTRANSPARENT_BRUSH);

    wxPen pen1(*wxWHITE);
    dc.SetPen(pen1);
    dc.DrawRectangle(physSelection);

    wxPen pen2(*wxBLACK, 1, wxPENSTYLE_DOT);
    dc.SetPen(pen2);
    dc.DrawRectangle(physSelection);

#endif
}

/// Converts the specified fragment of `src` to a 24-bit RGB bitmap.
static wxBitmap ImageToRgbBitmap(const c_Image& src, int x0, int y0, int width, int height)
{
    c_Image rgbImage = src.GetConvertedPixelFormatSubImage(PixelFormat::PIX_RGB8, x0, y0, width, height);
    // for storage, `rgbImage` uses `c_SimpleBuffer`, which has no row padding, so we can pass it directly to wxImage's constructor
    wxImage wximg(width, height, rgbImage.GetRowAs<unsigned char>(0), true);
    return wxBitmap(wximg);
}

void c_CpuAndBitmaps::RefreshRect(const wxRect& rect)
{
    m_ImgView.GetContentsPanel().RefreshRect(rect, false);
}

c_CpuAndBitmaps::c_CpuAndBitmaps(c_ScrolledView& imgView)
: m_ImgView(imgView)
{
    imgView.GetContentsPanel().Bind(wxEVT_PAINT, &c_CpuAndBitmaps::OnPaint, this);
    m_ScalingTimer.SetHandler([this]
    {
        if (m_ImgBmp)
        {
            CreateScaledPreview(m_NewZoomFactor);
            m_ImgView.GetContentsPanel().Refresh(m_ZoomFactor != m_NewZoomFactor);
            m_ZoomFactor = m_NewZoomFactor;
        }
    });
    m_EvtHandler.Bind(wxEVT_THREAD, &c_CpuAndBitmaps::OnThreadEvent, this);
}

void c_CpuAndBitmaps::ImageViewScrolledOrResized(float zoomFactor)
{
    m_NewZoomFactor = zoomFactor;
    if (m_Img && m_NewZoomFactor != ZOOM_NONE)
        m_ScalingTimer.StartOnce(IMAGE_SCALING_DELAY_MS);
}

void c_CpuAndBitmaps::ImageViewZoomChanged(float zoomFactor)
{
    m_ZoomFactor = zoomFactor;
    CreateScaledPreview(m_ZoomFactor);
}

void c_CpuAndBitmaps::FileOpened(c_Image&& img, std::optional<wxRect> newSelection)
{
    m_Img = std::move(img);
    m_ImgBmp = ImageToRgbBitmap(
        m_Img.value(),
        0,
        0,
        m_Img.value().GetWidth(),
        m_Img.value().GetHeight()
    );

    if (m_ZoomFactor != ZOOM_NONE)
        CreateScaledPreview(m_ZoomFactor);

    if (newSelection.has_value())
        m_Selection = newSelection.value();

    ScheduleProcessing(ProcessingRequest::SHARPENING);
}

void c_CpuAndBitmaps::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(&m_ImgView.GetContentsPanel());
    if (!m_ImgBmp)
        return;

    wxRegionIterator upd(m_ImgView.GetContentsPanel().GetUpdateRegion());
    wxMemoryDC imgDC(m_ImgBmp.value());

    if (m_ZoomFactor == ZOOM_NONE)
    {
        while (upd)
        {
            dc.Blit(wxPoint(upd.GetX(), upd.GetY()),
                wxSize(upd.GetWidth(), upd.GetHeight()),
                &imgDC, m_ImgView.CalcUnscrolledPosition(wxPoint(upd.GetX(), upd.GetY())));
            upd++;
        }
    }
    else
    {
        /*
            PAINTING WHEN ZOOM <> 1.0

            Values of different variables used are illustrated below:


            +----- m_ImgView: virtual size (m_ImgBmp * m_ZoomFactor) -------------------+
            |                                                                           |
            |                                                                           |
            |           +======= m_ImgView: visible portion ========================+   |
            |           |                                                           |   |
            |   +-------|---- updateArea (corresponds to m_BmpScaled) -----------+  |   |
            |   |       |                                                        |  |   |
            |   |       |                                                        |  |   |
            |   |       |  +---- updRect -----+                                  |  |   |
            |   |       |  |                  |                                  |  |   |
            |   |       |  +------------------+                                  |  |   |
            |   |       +===========================================================+   |
            |   |                                                                |      |
            |   |                                                                |      |
            |   +----------------------------------------------------------------+      |
            |                                                                           |
            |                                                                           |
            |                                                                           |
            |                                                                           |
            +---------------------------------------------------------------------------+

            When we are asked to paint over `updRect`, we must blit from `m_BmpScaled` (`imgDC`).
            This bitmap represents a scaled portion of `m_ImgBmp`, which does not necessarily correspond
            to the position of `m_ImgView`s' visible fragment at the moment. To find `srcPt`, which
            is the source point in `m_BmpScaled` to start blitting from, we must:
                - convert window (physical) left-top of `updRect` to logical one within `m_ImgView`
                - determine `updateArea` by reverse-scaling `m_ScaledArea`
                - express `updRect` in `m_BmpScaled` logical coordinates (rather than `m_ImgView`)
                  by subtracting the left-top of `updateArea` (which is expressed in `m_ImgView` logical coords)
        */

        wxMemoryDC imgDC(m_BmpScaled.value());
        wxRect updateArea = m_ScaledArea;
        updateArea.x *= m_ZoomFactor;
        updateArea.y *= m_ZoomFactor;
        updateArea.width *= m_ZoomFactor;
        updateArea.height *= m_ZoomFactor;

        while (upd)
        {
            wxRect updRect = upd.GetRect();
            wxPoint srcPt = m_ImgView.CalcUnscrolledPosition(updRect.GetTopLeft());
            srcPt.x -= updateArea.x;
            srcPt.y -= updateArea.y;
            dc.Blit(updRect.GetTopLeft(), updRect.GetSize(), &imgDC, srcPt);
            upd++;
        }
    }

    MarkSelection(m_PhysSelectionGetter(), dc);
}

void c_CpuAndBitmaps::CreateScaledPreview(float zoomFactor)
{
    const wxPoint scrollPos = m_ImgView.CalcUnscrolledPosition(wxPoint(0, 0));

    m_ScaledArea.SetLeft(scrollPos.x / zoomFactor);
    m_ScaledArea.SetTop(scrollPos.y / zoomFactor);
    const wxSize viewSize = m_ImgView.GetContentsPanel().GetSize();
    m_ScaledArea.SetWidth(viewSize.GetWidth() / zoomFactor);
    m_ScaledArea.SetHeight(viewSize.GetHeight() / zoomFactor);

    // limit the scaling request area to fit inside `m_ImgBmp`

    if (m_ScaledArea.x < 0)
        m_ScaledArea.x = 0;
    if (m_ScaledArea.x >= m_ImgBmp.value().GetWidth())
        m_ScaledArea.x = m_ImgBmp.value().GetWidth() - 1;
    if (m_ScaledArea.GetRight() >= m_ImgBmp.value().GetWidth())
        m_ScaledArea.SetRight(m_ImgBmp.value().GetWidth() - 1);

    if (m_ScaledArea.y < 0)
        m_ScaledArea.y = 0;
    if (m_ScaledArea.y >= m_ImgBmp.value().GetHeight())
        m_ScaledArea.y = m_ImgBmp.value().GetHeight() - 1;
    if (m_ScaledArea.GetBottom() >= m_ImgBmp.value().GetHeight())
        m_ScaledArea.SetBottom(m_ImgBmp.value().GetHeight() - 1);

    wxBitmap srcBmp = m_ImgBmp.value().GetSubBitmap(m_ScaledArea);
    m_BmpScaled = wxBitmap(srcBmp.ConvertToImage().Scale(
        srcBmp.GetWidth() * zoomFactor,
        srcBmp.GetHeight() * zoomFactor,
        wxIMAGE_QUALITY_NEAREST/*TODO! GetResizeQuality(s.scalingMethod)*/)
    );
}

static Histogram DetermineHistogram(const c_Image& img, const wxRect& selection)
{
    constexpr int NUM_HISTOGRAM_BINS = 1024;

    Histogram histogram{};

    histogram.values.insert(histogram.values.begin(), NUM_HISTOGRAM_BINS, 0);
    histogram.minValue = FLT_MAX;
    histogram.maxValue = -FLT_MAX;

    for (int y = 0; y < selection.height; y++)
    {
        const float* row = img.GetRowAs<float>(selection.y + y) + selection.x;
        for (int x = 0; x < selection.width; x++)
        {
            if (row[x] < histogram.minValue)
                histogram.minValue = row[x];
            else if (row[x] > histogram.maxValue)
                histogram.maxValue = row[x];

            const unsigned hbin = static_cast<unsigned>(row[x] * (NUM_HISTOGRAM_BINS - 1));
            IMPPG_ASSERT(hbin < NUM_HISTOGRAM_BINS);
            histogram.values[hbin] += 1;
        }
    }

    histogram.maxCount = 0;
    for (unsigned i = 0; i < histogram.values.size(); i++)
        if (histogram.values[i] > histogram.maxCount)
            histogram.maxCount = histogram.values[i];

    return histogram;
}

Histogram c_CpuAndBitmaps::GetHistogram()
{
    return DetermineHistogram(m_Img.value(), m_Selection);
}

void c_CpuAndBitmaps::NewSelection(const wxRect& selection)
{
    // restore unprocessed image contents in the previous selection
    wxBitmap restored = ImageToRgbBitmap(m_Img.value(), m_Selection.x, m_Selection.y, m_Selection.width, m_Selection.height);
    wxMemoryDC restoredDc(restored);
    wxMemoryDC(m_ImgBmp.value()).Blit(m_Selection.GetTopLeft(), m_Selection.GetSize(), &restoredDc, wxPoint(0, 0));

    if (m_ZoomFactor == ZOOM_NONE)
    {
        m_ImgView.GetContentsPanel().RefreshRect(wxRect(
            m_ImgView.CalcScrolledPosition(m_Selection.GetTopLeft()),
            m_ImgView.CalcScrolledPosition(m_Selection.GetBottomRight())),
            false);
    }
    else
    {
        // Restore also the corresponding fragment of the scaled bitmap.
        // Before restoring, increase the size of previous (unscaled) image fragment slightly to avoid any left-overs due to round-off errors.
        const int DELTA = std::max(6, static_cast<int>(std::ceil(1.0f / m_ZoomFactor)));

        // Area in `m_ImgBmp` to restore; based on `scaledSelection`, but limited to what is currently visible.
        wxRect selectionRst;
        // First, take the scaled selection and limit it to visible area.
        selectionRst = m_ScaledLogicalSelectionGetter();
        const wxPoint scrollPos = m_ImgView.GetScrollPos();
        const wxSize viewSize = m_ImgView.GetContentsPanel().GetSize();

        selectionRst.Intersect(wxRect(scrollPos, viewSize));

        wxRect scaledSelectionRst = selectionRst; // scaled area in `m_ImgView` (logical coords) to restore

        // second, scale it back to `m_ImgBmp` pixels
        selectionRst.x /= m_ZoomFactor;
        selectionRst.y /= m_ZoomFactor;
        selectionRst.width /= m_ZoomFactor;
        selectionRst.height /= m_ZoomFactor;

        selectionRst.Inflate(DELTA/2, DELTA/2);

        selectionRst.Intersect(wxRect(wxPoint(0, 0), m_ImgBmp.value().GetSize()));

        // also expand the scaled area to restore
        int scaledSelectionDelta = static_cast<int>(m_ZoomFactor * DELTA/2);
        scaledSelectionRst.Inflate(scaledSelectionDelta, scaledSelectionDelta);

        // create the scaled image fragment to restore
        wxBitmap restoredScaled(m_ImgBmp.value().GetSubBitmap(selectionRst).ConvertToImage().Scale(
            scaledSelectionRst.GetWidth(), scaledSelectionRst.GetHeight(),
            GetResizeQuality(/*s.scalingMethod*/ScalingMethod::NEAREST)));

        wxMemoryDC dcRestoredScaled(restoredScaled), dcScaled(m_BmpScaled.value());

        wxPoint destPt = scaledSelectionRst.GetTopLeft();
        destPt.x -= m_ScaledArea.x * m_ZoomFactor;
        destPt.y -= m_ScaledArea.y * m_ZoomFactor;
        dcScaled.Blit(
            destPt,
            restoredScaled.GetSize(),
            &dcRestoredScaled,
            wxPoint(0, 0)
        );

        wxRect updateReg(m_ImgView.CalcScrolledPosition(scaledSelectionRst.GetTopLeft()),
                         m_ImgView.CalcScrolledPosition(scaledSelectionRst.GetBottomRight()));
        m_ImgView.GetContentsPanel().RefreshRect(updateReg, false);

        wxRect prevSelPhys(m_ImgView.CalcScrolledPosition(m_ScaledLogicalSelectionGetter().GetTopLeft()),
                           m_ImgView.CalcScrolledPosition(m_ScaledLogicalSelectionGetter().GetBottomRight()));

        auto& cp = m_ImgView.GetContentsPanel();
        cp.RefreshRect(wxRect(prevSelPhys.GetLeft(), prevSelPhys.GetTop(), prevSelPhys.GetWidth(), 1),
            false);
        cp.RefreshRect(wxRect(prevSelPhys.GetLeft(), prevSelPhys.GetBottom(), prevSelPhys.GetWidth(), 1),
            false);
        cp.RefreshRect(wxRect(prevSelPhys.GetLeft(), prevSelPhys.GetTop(), 1, prevSelPhys.GetHeight()),
            false);
        cp.RefreshRect(wxRect(prevSelPhys.GetRight(), prevSelPhys.GetTop(), 1, prevSelPhys.GetHeight()),
            false);
    }

    m_Selection = selection;

    ScheduleProcessing(ProcessingRequest::SHARPENING);
}

void c_CpuAndBitmaps::NewProcessingSettings(const ProcessingSettings& procSettings)
{
    m_ProcSettings = procSettings;
    ScheduleProcessing(ProcessingRequest::SHARPENING);
}

void c_CpuAndBitmaps::LRSettingsChanged(const ProcessingSettings& procSettings)
{
    m_ProcSettings = procSettings;
    ScheduleProcessing(ProcessingRequest::SHARPENING);
}

void c_CpuAndBitmaps::UnshMaskSettingsChanged(const ProcessingSettings& procSettings)
{
    m_ProcSettings = procSettings;
    ScheduleProcessing(ProcessingRequest::UNSHARP_MASKING);
}

void c_CpuAndBitmaps::ToneCurveChanged(const ProcessingSettings& procSettings)
{
    m_ProcSettings = procSettings;
    ScheduleProcessing(ProcessingRequest::TONE_CURVE);
}

bool c_CpuAndBitmaps::IsProcessingInProgress()
{
    auto lock = m_Processing.worker.Lock();
    return lock.Get() != nullptr;
}

/// Aborts processing and schedules new processing to start ASAP (as soon as 'm_Processing.worker' is not running)
void c_CpuAndBitmaps::ScheduleProcessing(ProcessingRequest request)
{
    if (!m_Img) return;

    ProcessingRequest originalReq = request;

    // if the previous processing step(s) did not complete, we have to execute it (them) first
    if (request == ProcessingRequest::TONE_CURVE && !m_Processing.output.unsharpMasking.valid)
        request = ProcessingRequest::UNSHARP_MASKING;

    if (request == ProcessingRequest::UNSHARP_MASKING && !m_Processing.output.sharpening.valid)
        request = ProcessingRequest::SHARPENING;

    Log::Print(wxString::Format("Scheduling processing; requested: %d, scheduled: %d\n",
        static_cast<int>(originalReq),
        static_cast<int>(request))
    );

    m_Processing.processingRequest = request;

    if (!IsProcessingInProgress())
        StartProcessing();
    else
    {
        // Signal the worker thread to finish ASAP.
        { auto lock = m_Processing.worker.Lock();
            if (lock.Get())
            {
                Log::Print("Sending abort request to the worker thread\n");
                lock.Get()->AbortProcessing();
            }
        }

        // Set a flag so that we immediately restart the worker thread
        // after receiving the "processing finished" message.
        m_Processing.processingScheduled = true;
    }
}

void c_CpuAndBitmaps::OnProcessingStepCompleted(CompletionStatus status)
{
    // SetActionText(_("Idle"));

    if (m_Processing.processingRequest == ProcessingRequest::TONE_CURVE ||
        status == CompletionStatus::ABORTED)
    {
        if (m_Processing.processingRequest == ProcessingRequest::TONE_CURVE && status == CompletionStatus::COMPLETED)
        {
            m_Processing.output.toneCurve.preciseValuesApplied = m_Processing.usePreciseTCurveVals;
        }

        // This flag is set only for saving the output file. Clear it if the `TONE_CURVE` processing request
        // has finished for any reason or there was an abort regardless of the current request.
        m_Processing.usePreciseTCurveVals = false;
    }

    if (status == CompletionStatus::COMPLETED)
    {
        Log::Print("Processing step completed\n");

        if (m_Processing.processingRequest == ProcessingRequest::SHARPENING)
        {
            m_Processing.output.sharpening.valid = true;
            ScheduleProcessing(ProcessingRequest::UNSHARP_MASKING);
        }
        else if (m_Processing.processingRequest == ProcessingRequest::UNSHARP_MASKING)
        {
            m_Processing.output.unsharpMasking.valid = true;
            ScheduleProcessing(ProcessingRequest::TONE_CURVE);
        }
        else if (m_Processing.processingRequest == ProcessingRequest::TONE_CURVE)
        {
            m_Processing.output.toneCurve.valid = true;

            // all steps completed, draw the processed fragment
            UpdateSelectionAfterProcessing();

            if (m_OnProcessingCompleted)
                m_OnProcessingCompleted();
        }
    }
    else if (status == CompletionStatus::ABORTED)
        {};//m_CurrentSettings.m_FileSaveScheduled = false;
}

void c_CpuAndBitmaps::UpdateSelectionAfterProcessing()
{
    Log::Print("Updating selection after processing\n");

    wxBitmap updatedArea = ImageToRgbBitmap(m_Processing.output.toneCurve.img.value(), 0, 0,
        m_Processing.output.toneCurve.img.value().GetWidth(),
        m_Processing.output.toneCurve.img.value().GetHeight());

    wxMemoryDC dcUpdated(updatedArea), dcMain(m_ImgBmp.value());
    dcMain.Blit(m_Selection.GetTopLeft(), m_Selection.GetSize(), &dcUpdated, wxPoint(0, 0));
    // `updatedArea` needs to be deselected from DC before we can call GetSubBitmap() on it (see below)
    dcUpdated.SelectObject(wxNullBitmap);

    if (m_ZoomFactor == ZOOM_NONE)
    {
        m_ImgView.GetContentsPanel().RefreshRect(wxRect(
            m_ImgView.CalcScrolledPosition(m_Selection.GetTopLeft()),
            m_ImgView.CalcScrolledPosition(m_Selection.GetBottomRight())),
            false
        );
    }
    else if (m_BmpScaled)
    {
        // area in `updatedArea` to use; based on `scaledSelection`, but limited to what is currently visible
        wxRect selectionRst;
        // first, take the scaled selection and limit it to visible area
        selectionRst = m_ScaledLogicalSelectionGetter();
        const wxPoint scrollPos = m_ImgView.GetScrollPos();
        const wxSize viewSize = m_ImgView.GetContentsPanel().GetSize();

        selectionRst.Intersect(wxRect(scrollPos, viewSize));

        wxRect scaledSelectionRst = selectionRst; // scaled area in `m_ImgView` (logical coords) to restore

        // second, scale it back to `m_ImgBmp` pixels
        selectionRst.x /= m_ZoomFactor;
        selectionRst.y /= m_ZoomFactor;
        selectionRst.width /= m_ZoomFactor;
        selectionRst.height /= m_ZoomFactor;

        // third, translate it from `m_ImgBmp` to `updatedArea` coordinates
        selectionRst.SetPosition(selectionRst.GetPosition() - m_Selection.GetPosition());

        // limit `selectionRst` to fall within `updatedArea`
        selectionRst.Intersect(wxRect(wxPoint(0, 0), updatedArea.GetSize()));

        // the user could have scrolled the view during processing, check if anything is visible
        if (selectionRst.GetWidth() == 0 || selectionRst.GetHeight() == 0)
            return;

        wxBitmap updatedAreaScaled(((updatedArea.GetSubBitmap(selectionRst).ConvertToImage().Scale(
            scaledSelectionRst.GetWidth(), scaledSelectionRst.GetHeight(),
            GetResizeQuality(/*s.scalingMethod*/ScalingMethod::NEAREST)))));

        wxMemoryDC dcUpdatedScaled(updatedAreaScaled), dcScaled(m_BmpScaled.value());

        wxPoint destPt = scaledSelectionRst.GetTopLeft();
        destPt.x -= m_ScaledArea.x * m_ZoomFactor;
        destPt.y -= m_ScaledArea.y * m_ZoomFactor;
        dcScaled.Blit(
            destPt,
            scaledSelectionRst.GetSize(),
            &dcUpdatedScaled,
            wxPoint(0, 0)/*FIXME: add origin of scaledSelectionRst*/
        );
        wxRect updateRegion(
            m_ImgView.CalcScrolledPosition(scaledSelectionRst.GetTopLeft()),
            m_ImgView.CalcScrolledPosition(scaledSelectionRst.GetBottomRight())
        );
        m_ImgView.GetContentsPanel().RefreshRect(updateRegion, false);
    }

    // Histogram histogram;
    // // Show histogram of the results of all processing steps up to unsharp masking,
    // // but NOT including tone curve application.
    // DetermineHistogram(s.output.unsharpMasking.img.value(), wxRect(0, 0, s.selection.width, s.selection.height), histogram);
    // m_Ctrls.tcrvEditor->SetHistogram(histogram);

    // if (s.m_FileSaveScheduled)
    // {
    //     s.m_FileSaveScheduled = false;
    //     OnSaveFile();
    // }
}

void c_CpuAndBitmaps::StartLRDeconvolution()
{
    auto& img = m_Processing.output.sharpening.img;
    if (!img.has_value() ||
        static_cast<int>(img->GetWidth()) != m_Selection.width ||
        static_cast<int>(img->GetHeight()) != m_Selection.height)
    {
        img = c_Image(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
    }

    // invalidate the current output and those of subsequent steps
    m_Processing.output.sharpening.valid = false;
    m_Processing.output.unsharpMasking.valid = false;
    m_Processing.output.toneCurve.valid = false;

    if (m_ProcSettings.LucyRichardson.iterations == 0)
    {
        Log::Print("Sharpening disabled, no work needed\n");

        // No processing required, just copy the selection into `output.sharpening.img`,
        // as it will be used by the subsequent processing steps.

        c_Image::Copy(m_Img.value(), m_Processing.output.sharpening.img.value(),
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
                m_Processing.currentThreadId));

        // Sharpening thread takes the currently selected fragment of the original image as input
        m_Processing.worker = new c_LucyRichardsonThread(
            WorkerParameters{
                m_EvtHandler,
                m_Processing.worker,
                0, // in the future we will pass the index of the currently open image
                c_ImageBufferView(
                    m_Img.value().GetBuffer(),
                    m_Selection.x,
                    m_Selection.y,
                    m_Selection.width,
                    m_Selection.height
                ),
                m_Processing.output.sharpening.img.value().GetBuffer(),
                m_Processing.currentThreadId
            },
            m_ProcSettings.LucyRichardson.sigma,
            m_ProcSettings.LucyRichardson.iterations,
            m_ProcSettings.LucyRichardson.deringing.enabled,
            254.0f/255, true, m_ProcSettings.LucyRichardson.sigma
        );

        // SetActionText(wxString::Format(_(L"L\u2013R deconvolution") + ": %d%%", 0));
        { auto lock = m_Processing.worker.Lock();
            lock.Get()->Run();
        }
    }
}

void c_CpuAndBitmaps::StartUnsharpMasking()
{
    auto& img = m_Processing.output.unsharpMasking.img;
    if (!img.has_value() ||
        static_cast<int>(img->GetWidth()) != m_Selection.width ||
        static_cast<int>(img->GetHeight()) != m_Selection.height)
    {
        img = c_Image(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
    }

    // invalidate the current output and those of subsequent steps
    m_Processing.output.unsharpMasking.valid = false;
    m_Processing.output.toneCurve.valid = false;

    if (!m_ProcSettings.unsharpMasking.IsEffective())
    {
        Log::Print("Unsharp masking disabled, no work needed\n");

        // No processing required, just copy the selection into `output.sharpening.img`,
        // as it will be used by the subsequent processing steps.
        c_Image::Copy(m_Processing.output.sharpening.img.value(), m_Processing.output.unsharpMasking.img.value(), 0, 0, m_Selection.width, m_Selection.height, 0, 0);
        OnProcessingStepCompleted(CompletionStatus::COMPLETED);
    }
    else
    {
        Log::Print(wxString::Format("Launching unsharp masking worker thread (id = %d)\n",
            m_Processing.currentThreadId));

        // unsharp masking thread takes the output of sharpening as input
        m_Processing.worker = new c_UnsharpMaskingThread(
            WorkerParameters{
                m_EvtHandler,
                m_Processing.worker,
                0, // in the future we will pass the index of currently open image
                m_Processing.output.sharpening.img.value().GetBuffer(),
                m_Processing.output.unsharpMasking.img.value().GetBuffer(),
                m_Processing.currentThreadId
            },
            c_ImageBufferView(m_Img.value().GetBuffer(), m_Selection),
            m_ProcSettings.unsharpMasking.adaptive,
            m_ProcSettings.unsharpMasking.sigma,
            m_ProcSettings.unsharpMasking.amountMin,
            m_ProcSettings.unsharpMasking.amountMax,
            m_ProcSettings.unsharpMasking.threshold,
            m_ProcSettings.unsharpMasking.width
        );
        //SetActionText(wxString::Format(_("Unsharp masking: %d%%"), 0));
        { auto lock = m_Processing.worker.Lock();
            lock.Get()->Run();
        }
    }
}

void c_CpuAndBitmaps::StartToneCurve()
{
    auto& img = m_Processing.output.toneCurve.img;
    if (!img.has_value() ||
        static_cast<int>(img->GetWidth()) != m_Selection.width ||
        static_cast<int>(img->GetHeight()) != m_Selection.height)
    {
        img = c_Image(m_Selection.width, m_Selection.height, PixelFormat::PIX_MONO32F);
    }

    Log::Print("Created tone curve output image\n");

    // Invalidate the current output
    m_Processing.output.toneCurve.valid = false;

    if (m_ProcSettings.toneCurve.IsIdentity())
    {
        Log::Print("Tone curve is an identity map, no work needed\n");

        c_Image::Copy(
            m_Processing.output.unsharpMasking.img.value(),
            m_Processing.output.toneCurve.img.value(),
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
                m_Processing.currentThreadId));

        // tone curve thread takes the output of unsharp masking as input

        m_Processing.worker = new c_ToneCurveThread(
            WorkerParameters{
                m_EvtHandler,
                m_Processing.worker,
                0, // in the future we will pass the index of currently open image
                m_Processing.output.unsharpMasking.img.value().GetBuffer(),
                m_Processing.output.toneCurve.img.value().GetBuffer(),
                m_Processing.currentThreadId
            },
            m_ProcSettings.toneCurve,
            m_Processing.usePreciseTCurveVals
        );
        //SetActionText(wxString::Format(_("Applying tone curve: %d%%"), 0));
        { auto lock = m_Processing.worker.Lock();
            lock.Get()->Run();
        }
    }
}

void c_CpuAndBitmaps::StartProcessing()
{
    IMPPG_ASSERT(m_Img);

    Log::Print("Starting processing\n");

    // sanity check; the background thread should be finished and deleted at this point
    if (IsProcessingInProgress())
    {
        Log::Print("WARNING: The worker thread is still running!\n");
        return;
    }

    m_Processing.processingScheduled = false;

    // Make sure that if there are outdated thread events out there, they will be recognized
    // as such and discarded (`currentThreadId` will be sent from worker in event.GetInt()).
    // See also: OnThreadEvent().
    m_Processing.currentThreadId += 1;

    switch (m_Processing.processingRequest)
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

void c_CpuAndBitmaps::OnThreadEvent(wxThreadEvent& event)
{
    // On rare occasions it may happen that the event is outdated and had been sent
    // by a previously launched worker thread, which has already deleted itself.
    // In such case, ignore the event.
    if (event.GetInt() != m_Processing.currentThreadId)
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
            if (m_Processing.processingRequest == ProcessingRequest::SHARPENING)
                action = _(L"Lucy\u2013Richardson deconvolution");
            else if (m_Processing.processingRequest == ProcessingRequest::UNSHARP_MASKING)
                action = _("Unsharp masking");
            else if (m_Processing.processingRequest == ProcessingRequest::TONE_CURVE)
                action = _("Applying tone curve");

            //SetActionText(wxString::Format(action + ": %d%%", event.GetPayload<WorkerEventPayload>().percentageComplete));
            break;
        }

    case ID_FINISHED_PROCESSING:
        {
            const WorkerEventPayload &p = event.GetPayload<WorkerEventPayload>();

            Log::Print(wxString::Format("Received a processing completion event from threadId = %d, status = %s\n",
                    event.GetInt(), p.completionStatus == CompletionStatus::COMPLETED ? "COMPLETED" : "ABORTED"));

            OnProcessingStepCompleted(p.completionStatus);

            if (m_Processing.processingScheduled)
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

} // namespace imppg::backend
