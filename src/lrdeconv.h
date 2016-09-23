/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2015, 2016 Filip Szczerek <ga.software@yahoo.com>

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

#include <cstdint>
#include <boost/function.hpp>
#include "image.h"

typedef enum
{
    CONV_AUTO,           ///< Automatically select CONV_STANDARD or CONV_YOUNG_VAN_VLIET depending on "sigma"
    CONV_STANDARD,       ///< Standard iterative convolution using 1D kernel projection (1D convolution of rows and columns)
    CONV_YOUNG_VAN_VLIET ///< Young & van Vliet recursive Gaussian convolution
} ConvolutionMethod_t;

/// Wrapper for an array which may contain row padding. Stores only the pointer and dimensions; can be copied, deleted without influencing the allocated memory.
template<typename T>
class c_PaddedArrayPtr
{
    T *m_Array;
    int m_Width, m_Height, m_BytesPerRow;

public:
    c_PaddedArrayPtr(T *start, int width, int height, int bytesPerRow = 0): m_Array(start), m_Width(width), m_Height(height), m_BytesPerRow(bytesPerRow)
    {
        if (bytesPerRow == 0)
            m_BytesPerRow = width * sizeof(T);
    }

    /// Gets element at [row][col]
    T &get(int row, int col) { return *((uint8_t *)m_Array + row*m_BytesPerRow + col * sizeof(T)); }

    /// Returns pointer to the specified row
    T *row(int row) { return (T *)((uint8_t *)m_Array + row*m_BytesPerRow); }

    int width() { return m_Width; }
    int height() { return m_Height; }
    int GetBytesPerRow() { return m_BytesPerRow; }
};

/// Clamps the values of the specified floating-point buffer to [0.0, 1.0]
void Clamp(float array[], int width, int height, int bytesPerRow);

/// Calculates convolution of 'input' with a Gaussian kernel
void ConvolveSeparable(
        c_PaddedArrayPtr<float> input,  ///< Input array
        c_PaddedArrayPtr<float> output, ///< Output array having as much rows and columns as 'input' does
        float sigma              ///< Gaussian sigma
);

/// Reproduces original image from image in 'input' convolved with Gaussian kernel and writes it to 'output'.
void LucyRichardsonGaussian(
        IImageBuffer &input, ///< Contains a single 'float' value per pixel; size the same as 'output'
        IImageBuffer &output, ///< Contains a single 'float' value per pixel; size the same as 'input'
        int numIters,  ///< Number of iterations
        float sigma,   ///< sigma of the Gaussian kernel
        ConvolutionMethod_t convMethod,
        /// Called after every iteration; arguments: current iteration, total iterations
        boost::function<void(int, int)> progressCallback,
        boost::function<bool()> checkAbort ///< Called periodically to check if there was an "abort processing" request
);

/// Blurs pixels around borders of brightness areas defined by 'threshold'
void BlurThresholdVicinity(
    IImageBuffer &input,
    IImageBuffer &output,
    float threshold, ///< Threshold to qualify pixels as "border pixels"
    bool greaterThan,
    float sigma
);

#endif // IMPP_LRDECONV_H
