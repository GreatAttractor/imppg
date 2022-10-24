/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2022 Filip Szczerek <ga.software@yahoo.com>

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
    Convolution header.
*/

#pragma once

#include <cstdint>

enum class ConvolutionMethod
{
    AUTO,           ///< Automatically select STANDARD or YOUNG_VAN_VLIET depending on "sigma"
    STANDARD,       ///< Standard iterative convolution using 1D kernel projection (1D convolution of rows and columns)
    YOUNG_VAN_VLIET ///< Young & van Vliet recursive Gaussian convolution
};

/** Minimum radius in pixels (= ceil(3*sigma)) of the Gaussian kernel for which
    the Young & van Vliet recursive convolution is to be used. Below that,
    the standard iterative implementation is currently faster (as tested on my
    Core i5-3570K with DDR3 PC-10700 RAM, compiled with MS C++ 18.00, for 1-4 threads - Filip). */
constexpr int YOUNG_VAN_VLIET_MIN_KERNEL_RADIUS = 8;

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
        return reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(m_Array) + row * m_BytesPerRow);
    }

    const T* row_const(int row) const
    {
        return reinterpret_cast<const T*>(reinterpret_cast<const std::uint8_t*>(m_Array) + row * m_BytesPerRow);
    }

    int width() const { return m_Width; }
    int height() const { return m_Height; }
    int GetBytesPerRow() const { return m_BytesPerRow; }
};

/// Calculates convolution of 'input' with a Gaussian kernel
void ConvolveSeparable(
    c_PaddedArrayPtr<const float> input, ///< Input array.
    c_PaddedArrayPtr<float> output,      ///< Output array having as much rows and columns as 'input' does.
    float sigma,                         ///< Gaussian sigma.
    std::size_t numChannels
);

/// Calculates convolution of `input` with a rotationally symmetric and separable (i.e., Gaussian) `kernel`
/// and writes it in transposed form to `output`.
void ConvolveSeparableTranspose(
    c_PaddedArrayPtr<const float> input,  ///< Input array.
    /// Transposed output array; contains as many rows as `input` does columns and as many columns as `input` does rows.
    c_PaddedArrayPtr<float> output,
    /// Contains convolution kernel's projection (horizontal/vertical); element `[kernelRadius]` is the middle.
    const float kernel[],
    int kernelRadius, ///< `kernel` contains 2*kernelRadius-1 elements.
    float tempBuf1[], ///< Temporary buffer 1, as many elements as `input`.
    float tempBuf2[], ///< Temporary buffer 2, as many elements as `input`.
    std::size_t numChannels
);

/// Calculates convolution of `input` with an approximated Gaussian kernel (Young & van Vliet recursive method)
/// and writes it in transposed form to `output`.
void ConvolveGaussianRecursiveTranspose(
    /// Input array.
    c_PaddedArrayPtr<const float> input,
    /// Transposed output array; contains as many rows as `input` does columns and as many columns as `input` does rows.
    c_PaddedArrayPtr<float> output,
    float sigma,      ///< Gaussian sigma.
    float tempBuf1[], ///< Contains width*height elements.
    float tempBuf2[], ///< Contains width*height elements.
    std::size_t numChannels
);

/// Matrices are transposed in square blocks of this length to a side
constexpr int TRANSPOSITION_BLOCK_SIZE = 18;
static_assert(TRANSPOSITION_BLOCK_SIZE % 3 == 0, "block size must be divisible by 3 for RGB processing");

/// Transposes matrix `input` and writes it to `output`.
template<typename T, std::size_t NumChannels>
void Transpose(
    const T* input,
    T* output,
    int width, ///< Number of columns in `input`, rows in `output`.
    int height, ///< Number of rows in `input`, columns in `output`.
    std::size_t inputValuesPerRow,
    std::size_t outputValuesPerRow,
    int blockSize
)
{
    static_assert(false, "write unit tests for this");
    // Transpose blocks.

    // i, j - indices of the current block
    for (int j = 0; j < height / blockSize; j++)
    {
        for (int i = 0; i < width / blockSize; i++)
        {
            const T* inBlk = input + (i * NumChannels + j * inputValuesPerRow) * blockSize;
            T* outBlk = output + (j * NumChannels + i * outputValuesPerRow) * blockSize;

            // x, y - indices of the current element within the current block
            for (int y = 0; y < blockSize; y++)
            {
                for (int x = 0; x < blockSize; x++)
                {
                    for (std::size_t ch = 0; ch < NumChannels; ++ch)
                    {
                        outBlk[y * NumChannels + ch + x * outputValuesPerRow] = inBlk[x * NumChannels + ch + y * inputValuesPerRow];
                    }
                }
            }
        }
    }

    // Transpose the remaining elements outside the whole blocks.

    // rightmost remaining elements
    for (int j = 0; j < height - (height % blockSize); j++)
        for (int i = width - (width % blockSize); i < width; i++)
        {
            for (std::size_t ch = 0; ch < NumChannels; ++ch)
            {
                output[j * NumChannels + ch + i * outputValuesPerRow] = input[i * NumChannels + ch + j * inputValuesPerRow];
            }
        }

    // bottommost remaining elements
    for (int j = 0; j < width - (width % blockSize); j++)
        for (int i = height - (height % blockSize); i < height; i++)
        {
            //TODO
            // *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(output) + i*sizeof(T) + j*outputBytesPerRow) =
            //     *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(input) + j*sizeof(T) + i*inputBytesPerRow);
        }

    // bottom-right residual rectangle
    for (int j = height - (height % blockSize); j < height; j++)
        for (int i = width - (width % blockSize); i < width; i++)
        {
            //TODO
            // *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(output) + j*sizeof(T) + i*outputBytesPerRow) =
            //     *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(input) + i*sizeof(T) + j*inputBytesPerRow);
        }
}
