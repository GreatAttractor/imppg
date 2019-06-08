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

#include <optional>
#include <wx/bitmap.h>

#include "../backend.h"

namespace imppg::backend {

class CpuAndBitmaps: public IBackEnd
{
public:
    CpuAndBitmaps(wxScrolledCanvas& imgView);

    // Events -------------------------------------------------
    void ImageViewScrolled(const wxScrolledCanvas& imgView) override;
    void ImageViewZoomChanged(float zoomFactor) override;
    void FileOpened(c_Image&& img) override;

private:
    wxScrolledCanvas& m_ImgView;
    std::optional<c_Image> m_Img;
    std::optional<wxBitmap> m_ImgBmp; ///< Bitmap which wraps `m_Img` for displaying on `m_ImgView`.

    void OnPaint(wxPaintEvent& event);
};

} // namespace imppg::backend
