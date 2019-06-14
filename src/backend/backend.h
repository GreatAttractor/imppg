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

#include <functional>
#include <wx/scrolwin.h>

#include "../common.h"
#include "../image.h"

namespace imppg::backend {

class IBackEnd
{
public:
    virtual void ImageViewScrolledOrResized(float zoomFactor) = 0;

    virtual void ImageViewZoomChanged(float zoomFactor) = 0;

    virtual void FileOpened(c_Image&& img) = 0;

    /// Shall start processing of the selected image fragment immediately.
    virtual void NewSelection(
        const wxRect& selection ///< New selection for processing, in logical image coords.
    ) = 0;

    virtual void SetProcessingCompletedHandler(std::function<void()>) = 0;

    /// Provides getter of selection in physical image view coords.
    virtual void SetPhysicalSelectionGetter(std::function<wxRect()> getter) = 0;

    /// Returns histogram of current selection after before applying tone curve.
    virtual Histogram GetHistogram() = 0;

    virtual ~IBackEnd() = default;
};

} // namespace imppg

#endif // IMPPG_BACKEND_HEADER
