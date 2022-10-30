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
    CPU & bitmaps back end core implementation.
*/

#include <cfloat>
#include <wx/dcclient.h>
#include <wx/dcmemory.h>

//#include "ctrl_ids.h"
#include "logging/logging.h"
#include "cpu_bmp/cpu_bmp.h"

namespace imppg::backend {

/// Delay after a scroll or resize event before refreshing the display if zoom level <> 100%.
constexpr int IMAGE_SCALING_DELAY_MS = 150;

std::unique_ptr<IDisplayBackEnd> CreateCpuBmpDisplayBackend(c_ScrolledView& imgView)
{
    return std::make_unique<c_CpuAndBitmaps>(imgView);
}

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
    imgView.EnableContentsScrolling();

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

    m_Processor.SetProcessingCompletedHandler([this](CompletionStatus status) {
        if (status == CompletionStatus::COMPLETED)
        {
            UpdateSelectionAfterProcessing();
        }
        if (m_OnProcessingCompleted)
        {
            m_OnProcessingCompleted(status);
        }
    });
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
    if (m_Img.has_value())
    {
        CreateScaledPreview(m_ZoomFactor);
    }
}

void c_CpuAndBitmaps::SetImage(c_Image&& img, std::optional<wxRect> newSelection)
{
    IMPPG_ASSERT(
        img.GetPixelFormat() == PixelFormat::PIX_MONO32F ||
        img.GetPixelFormat() == PixelFormat::PIX_RGB32F
    );

    m_Img = std::move(img);

    m_ImgBmp = ImageToRgbBitmap(
        *m_Img,
        0,
        0,
        m_Img->GetWidth(),
        m_Img->GetHeight()
    );

    if (m_ZoomFactor != ZOOM_NONE)
        CreateScaledPreview(m_ZoomFactor);

    if (newSelection.has_value())
        m_Selection = newSelection.value();

    m_Processor.SetImage(m_Img.value());
    if (newSelection.has_value())
    {
        m_Processor.SetSelection(newSelection.value());
    }
    m_Processor.ScheduleProcessing(ProcessingRequest::SHARPENING);
}

void c_CpuAndBitmaps::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(&m_ImgView.GetContentsPanel());
    if (!m_ImgBmp)
        return;

    if (m_ZoomFactor == ZOOM_NONE)
    {
        wxRegionIterator upd(m_ImgView.GetContentsPanel().GetUpdateRegion());
        wxMemoryDC imgDC(m_ImgBmp.value());

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

        wxRegionIterator upd(m_ImgView.GetContentsPanel().GetUpdateRegion());
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

    const wxRect currentPhysSel = m_PhysSelectionGetter();
    const bool resizingWithWindowFit =
        m_ScalingTimer.IsRunning() &&
        m_PreviouslyMarkedSelection.has_value() &&
        (m_PreviouslyMarkedSelection->width != currentPhysSel.width ||
        m_PreviouslyMarkedSelection->height != currentPhysSel.height);

    if (!resizingWithWindowFit)
    {
        m_PreviouslyMarkedSelection = currentPhysSel;
        MarkSelection(currentPhysSel, dc);
    }
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
        GetResizeQuality(m_ScalingMethod))
    );
}

Histogram c_CpuAndBitmaps::GetHistogram()
{
    if (!m_Img)
        return Histogram{};

    if (const auto unshMaskResult = m_Processor.GetUnshMaskOutput())
    {
        return DetermineHistogramFromChannels(*unshMaskResult.value(), unshMaskResult.value()->at(0).GetImageRect());
    }
    else
    {
        return DetermineHistogram(*m_Img, m_Selection);
    }
}

void c_CpuAndBitmaps::NewSelection(
    const wxRect& selection,
    const wxRect& prevScaledLogicalSelection
)
{
    // restore unprocessed image contents in the previous selection
    wxBitmap restored = ImageToRgbBitmap(*m_Img, m_Selection.x, m_Selection.y, m_Selection.width, m_Selection.height);
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
        selectionRst = prevScaledLogicalSelection;
        const wxPoint scrollPos = m_ImgView.GetScrollPosition();
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
            GetResizeQuality(m_ScalingMethod)));

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
    m_Processor.SetSelection(selection);
    m_Processor.ScheduleProcessing(ProcessingRequest::SHARPENING);
}

void c_CpuAndBitmaps::NewProcessingSettings(const ProcessingSettings& procSettings)
{
    m_Processor.SetProcessingSettings(procSettings);
    m_Processor.ScheduleProcessing(ProcessingRequest::SHARPENING);
}

void c_CpuAndBitmaps::LRSettingsChanged(const ProcessingSettings& procSettings)
{
    m_Processor.SetProcessingSettings(procSettings);
    m_Processor.ScheduleProcessing(ProcessingRequest::SHARPENING);
}

void c_CpuAndBitmaps::UnshMaskSettingsChanged(const ProcessingSettings& procSettings)
{
    m_Processor.SetProcessingSettings(procSettings);
    m_Processor.ScheduleProcessing(ProcessingRequest::UNSHARP_MASKING);
}

void c_CpuAndBitmaps::ToneCurveChanged(const ProcessingSettings& procSettings)
{
    m_Processor.SetProcessingSettings(procSettings);
    m_Processor.ScheduleProcessing(ProcessingRequest::TONE_CURVE);
}

void c_CpuAndBitmaps::AbortProcessing()
{
    m_Processor.AbortProcessing();
}

void c_CpuAndBitmaps::UpdateSelectionAfterProcessing()
{
    Log::Print("Updating selection after processing\n");

    const c_Image& toneMappingOutput = m_Processor.GetProcessedOutput();
    wxBitmap updatedArea = ImageToRgbBitmap(toneMappingOutput, 0, 0,
        toneMappingOutput.GetWidth(),
        toneMappingOutput.GetHeight());

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
        const wxPoint scrollPos = m_ImgView.GetScrollPosition();
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
            GetResizeQuality(m_ScalingMethod)))));

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
}

void c_CpuAndBitmaps::SetScalingMethod(ScalingMethod scalingMethod)
{
    m_ScalingMethod = scalingMethod;
    if (m_ZoomFactor != ZOOM_NONE)
    {
        CreateScaledPreview(m_ZoomFactor);
        m_ImgView.GetContentsPanel().Refresh();
        m_ImgView.GetContentsPanel().Update();
    }
}

c_CpuAndBitmaps::~c_CpuAndBitmaps()
{
    m_ImgView.GetContentsPanel().Unbind(wxEVT_PAINT, &c_CpuAndBitmaps::OnPaint, this);
    m_ImgView.EnableContentsScrolling(false);
}

c_Image c_CpuAndBitmaps::GetProcessedSelection()
{
    IMPPG_ASSERT(m_Img.has_value());

    AbortProcessing();

    m_Processor.ApplyPreciseToneCurveValues();
    return m_Processor.GetProcessedOutput();
}

bool c_CpuAndBitmaps::ProcessingInProgress()
{
    return m_Processor.IsProcessingInProgress();
}

} // namespace imppg::backend
