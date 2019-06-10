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

#include <wx/dcclient.h>
#include <wx/dcmemory.h>

#include "cpu_bmp.h"

namespace imppg::backend {

/// Delay after a scroll or resize event before refreshing the display if zoom level <> 100%.
constexpr int IMAGE_SCALING_DELAY_MS = 150;

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

c_CpuAndBitmaps::c_CpuAndBitmaps(wxScrolledCanvas& imgView)
: m_ImgView(imgView)
{
    imgView.Bind(wxEVT_PAINT, &c_CpuAndBitmaps::OnPaint, this);
    m_ScalingTimer.SetHandler([this]
    {
        if (m_ImgBmp)
        {
            CreateScaledPreview(m_NewZoomFactor);
            m_ImgView.Refresh(m_ZoomFactor != m_NewZoomFactor);
            m_ZoomFactor = m_NewZoomFactor;
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
    CreateScaledPreview(m_ZoomFactor);
}

void c_CpuAndBitmaps::FileOpened(c_Image&& img)
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
}

void c_CpuAndBitmaps::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(&m_ImgView);
    if (!m_ImgBmp)
        return;

    wxRegionIterator upd(m_ImgView.GetUpdateRegion());
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
    const wxSize viewSize = m_ImgView.GetSize();
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

Histogram c_CpuAndBitmaps::GetHistogram()
{
    return Histogram{};
}

} // namespace imppg::backend
