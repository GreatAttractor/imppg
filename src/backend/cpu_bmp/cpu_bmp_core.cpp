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

/// Converts the specified fragment of `src` to a 24-bit RGB bitmap.
static wxBitmap ImageToRgbBitmap(const c_Image& src, int x0, int y0, int width, int height)
{
    c_Image rgbImage = src.GetConvertedPixelFormatSubImage(PixelFormat::PIX_RGB8, x0, y0, width, height);
    // for storage, `rgbImage` uses `c_SimpleBuffer`, which has no row padding, so we can pass it directly to wxImage's constructor
    wxImage wximg(width, height, rgbImage.GetRowAs<unsigned char>(0), true);
    return wxBitmap(wximg);
}

CpuAndBitmaps::CpuAndBitmaps(wxScrolledCanvas& imgView)
: m_ImgView(imgView)
{
    imgView.Bind(wxEVT_PAINT, &CpuAndBitmaps::OnPaint, this);
}

void CpuAndBitmaps::ImageViewScrolled(const wxScrolledCanvas& /*imgView*/)
{
}

void CpuAndBitmaps::ImageViewZoomChanged(float /*zoomFactor*/)
{
}

void CpuAndBitmaps::FileOpened(c_Image&& img)
{
    m_Img = std::move(img);
    m_ImgBmp = ImageToRgbBitmap(
        m_Img.value(),
        0,
        0,
        m_Img.value().GetWidth(),
        m_Img.value().GetHeight()
    );
}

void CpuAndBitmaps::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(&m_ImgView);
    if (!m_ImgBmp)
        return;

    wxRegionIterator upd(m_ImgView.GetUpdateRegion());
    wxMemoryDC imgDC(m_ImgBmp.value());

    while (upd)
    {
        dc.Blit(wxPoint(upd.GetX(), upd.GetY()),
            wxSize(upd.GetWidth(), upd.GetHeight()),
            &imgDC, m_ImgView.CalcUnscrolledPosition(wxPoint(upd.GetX(), upd.GetY())));
        upd++;
    }

    // selection in physical (m_ImgView) coordinates
    // wxRect physSelection(
    //     m_ImageView.CalcScrolledPosition(currSel.GetTopLeft()),
    //     m_ImageView.CalcScrolledPosition(currSel.GetBottomRight()));
    // MarkSelection(physSelection, dc);
}

} // namespace imppg::backend
