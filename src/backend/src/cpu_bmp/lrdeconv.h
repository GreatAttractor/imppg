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
    Lucy-Richardson deconvolution implementation header.
*/

#ifndef IMPP_LRDECONV_H
#define IMPP_LRDECONV_H

#include "image/image.h"
#include "math_utils/convolution.h"

#include <cstdint>
#include <functional>

/// Clamps the values of the specified PIX_MONO32F buffer to [0.0, 1.0]
void Clamp(c_View<IImageBuffer>& buf);

/// Calculates convolution of 'input' with a Gaussian kernel
void ConvolveSeparable(
        c_PaddedArrayPtr<const float> input, ///< Input array
        c_PaddedArrayPtr<float> output,      ///< Output array having as much rows and columns as 'input' does
        float sigma                           ///< Gaussian sigma
);

/// Reproduces original image from image in 'input' convolved with Gaussian kernel and writes it to 'output'.
void LucyRichardsonGaussian(
        c_View<const IImageBuffer>& input, ///< Contains a single 'float' value per pixel; size the same as 'output'
        c_View<IImageBuffer>& output, ///< Contains a single 'float' value per pixel; size the same as 'input'
        int numIters,  ///< Number of iterations
        float sigma,   ///< sigma of the Gaussian kernel
        ConvolutionMethod convMethod,

        /// Called after every iteration; arguments: current iteration, total iterations
        //boost::function<void(int, int)> progressCallback,
        std::function<void (int, int)> progressCallback,

        /// Called periodically to check if there was an "abort processing" request
        //boost::function<bool()> checkAbort
        std::function<bool ()> checkAbort
);

// c_Image GetTresholdVicinityMask(
//     c_View<const IImageBuffer> input,
//     float threshold, ///< Threshold to qualify pixels as "border pixels".
//     bool greaterThan ///< If true, returns mask with pixels > threshold.
// );


/// Blurs pixels around borders of brightness areas defined by 'threshold'
void BlurThresholdVicinity(
    c_View<const IImageBuffer> input,
    c_View<IImageBuffer> output,
    std::vector<uint8_t>& workBuf,
    float threshold, ///< Threshold to qualify pixels as "border pixels".
    float sigma
);

#endif // IMPP_LRDECONV_H
