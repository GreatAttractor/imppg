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
    Image alignment via phase correlation functions header.
*/

#ifndef IMPPG_PHASE_CORRELATION_ALIGNMENT_HEADER
#define IMPPG_PHASE_CORRELATION_ALIGNMENT_HEADER

#include <functional>
#include <memory>
#include <vector>
#include <wx/arrstr.h>
#include <wx/string.h>

#include "common/common.h"
#include "image/image.h"


/// Returns the smallest power of 2 which is > n
unsigned GetClosestGPowerOf2(unsigned n);

/// Determines translation vectors of an image sequence
bool DetermineTranslationVectors(
        unsigned Nwidth, ///< FFT width
        unsigned Nheight, ///< FFT height
        const wxArrayString& inputFiles, ///< List of input file names
        /// Receives list of translation vectors between files in 'inputFiles'; each vector is a translation relative to the first image
        std::vector<FloatPoint_t>& translation,
        /// Receives the bounding box (within the NxN working area) of all images after alignment
        Rectangle_t& bBox,
        // (Note: an untranslated image starts in the working area at (N-imgWidth)/2, (N-imgHeight)/2)
        std::string* errorMsg, ///< If not null, receives error message (if any)
        /// Called after determining an image's translation; argument: index of the current image
        bool subpixelAlignment,
        std::function<void (int, float, float)> progressCallback, ///< Called after determining translation of an image; arguments: image index, trans. vector
        std::function<bool ()> checkAbort ///< Called periodically to check if there was an "abort processing" request
);

/// Returns the set-theoretic intersection, i.e. the largest shared area, of specified images
Rectangle_t DetermineImageIntersection(
        unsigned Nwidth,    ///< Width of the working buffer (i.e. FFT arrays)
        unsigned Nheight,   ///< Height of the working buffer (i.e. FFT arrays)
        const std::vector<FloatPoint_t>& translation, ///< Translation vectors relative to the first image
        const std::vector<Point_t>& imgSize       ///< Image sizes
);

/// Determines translation vector between specified images; the images have to be already multiplied by window function
FloatPoint_t DetermineTranslationVector(
    const c_Image& img1, ///< Width and height have to be the same as 'img2' and be powers of two
    const c_Image& img2  ///< Width and height have to be the same as 'img1' and be powers of two
);

/// Calculates window function (Blackman) and returns its values as a PIX_MONO32F image
c_Image CalcWindowFunction(
    int wndWidth,
    int wndHeight
);

#endif // IMPPG_PHASE_CORRELATION_ALIGNMENT_HEADER
