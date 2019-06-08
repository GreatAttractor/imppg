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

#include <cstdint>
#include <functional>

#include "image.h"


enum class ConvolutionMethod
{
    AUTO,           ///< Automatically select STANDARD or YOUNG_VAN_VLIET depending on "sigma"
    STANDARD,       ///< Standard iterative convolution using 1D kernel projection (1D convolution of rows and columns)
    YOUNG_VAN_VLIET ///< Young & van Vliet recursive Gaussian convolution
};

/// Wrapper for an array which may contain row padding. Stores only the pointer and dimensions; can be copied, deleted without influencing the allocated memory.
template<typename T>
class c_PaddedArrayPtr
{
    T* m_Array;
    int m_Width, m_Height, m_BytesPerRow;

public:
    c_PaddedArrayPtr(T* start, int width, int height, int bytesPerRow = 0): m_Array(start), m_Width(width), m_Height(height), m_BytesPerRow(bytesPerRow)
    {
        if (bytesPerRow == 0)
            m_BytesPerRow = width * sizeof(T);
    }

    T* row(int row)
    {
        return reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(m_Array) + row * m_BytesPerRow);
    }

    T* row_const(int row) const
    {
        return reinterpret_cast<T*>(reinterpret_cast<const uint8_t*>(m_Array) + row * m_BytesPerRow);
    }

    int width() const { return m_Width; }
    int height() const { return m_Height; }
    int GetBytesPerRow() const { return m_BytesPerRow; }
};

/// Clamps the values of the specified PIX_MONO32F buffer to [0.0, 1.0]
void Clamp(c_ImageBufferView& buf);

/// Calculates convolution of 'input' with a Gaussian kernel
void ConvolveSeparable(
        c_PaddedArrayPtr<const float> input, ///< Input array
        c_PaddedArrayPtr<float> output,      ///< Output array having as much rows and columns as 'input' does
        float sigma                           ///< Gaussian sigma
);

/// Reproduces original image from image in 'input' convolved with Gaussian kernel and writes it to 'output'.
void LucyRichardsonGaussian(
        const c_ImageBufferView& input, ///< Contains a single 'float' value per pixel; size the same as 'output'
        c_ImageBufferView& output, ///< Contains a single 'float' value per pixel; size the same as 'input'
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

/// Blurs pixels around borders of brightness areas defined by 'threshold'
void BlurThresholdVicinity(
    const c_ImageBufferView& input,
    c_ImageBufferView& output,
    float threshold, ///< Threshold to qualify pixels as "border pixels"
    bool greaterThan,
    float sigma
);

#endif // IMPP_LRDECONV_H
