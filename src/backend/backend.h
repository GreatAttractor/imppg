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

#include <wx/scrolwin.h>

#include "../image.h"

namespace imppg::backend {

class IBackEnd
{
public:
    // Events -------------------------------------------------
    virtual void ImageViewScrolledOrResized(float zoomFactor) = 0;
    virtual void ImageViewZoomChanged(float zoomFactor) = 0;
    virtual void FileOpened(c_Image&& img) = 0;

    virtual ~IBackEnd() = default;
};

} // namespace imppg

#endif // IMPPG_BACKEND_HEADER
