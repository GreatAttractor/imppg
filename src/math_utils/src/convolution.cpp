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
    Convolution implementation.
*/

#include "math_utils/convolution.h"
#include "math_utils/gauss.h"

#include <cmath>
#include <cstring>
#include <memory>

#include "../../imppg_assert.h"

/// Performs a single step of 1D convolution using the middle kernel value 'kernelVal'
inline void Convolve1Dstep_OfsZero(const float input[], float output[], int len, float kernelVal)
{
    for (int i = 0; i < len; i++)
    {
        float influence = input[i] * kernelVal;
        output[i] += influence;
    }
}

/// Performs a single step of 1D convolution using the kernel value 'kernelVal', offset by 'kernelOfs' elements from kernel's middle
inline void Convolve1Dstep_OfsNonZero(const float input[], float output[], int len, float kernelVal, int kernelOfs)
{
    for (int i = 0; i < len; i++)
    {
        float influence = input[i] * kernelVal;
        output[i + kernelOfs] += influence;
        output[i - kernelOfs] += influence;
    }
}


/// Performs a Young & van Vliet approximated recursive Gaussian filtering of values in one direction
inline void YvVFilterValues(
    const float input[], ///< Input array
    float output[], ///< Output array (may equal 'input')
    int length, ///< Number of elements in 'input', 'output'
    int direction, ///< 1: filter forward, -1: filter backward; if -1, processing starts at the last element
    // YvV coefficients
    float b0inv, float b1, float b2, float b3, float B
)
{
    int startIdx; // Starting index to process (inclusive)
    int endIdx; // End index to process (exclusive)
    if (direction == 1)
    {
        startIdx = 0;
        endIdx = length;
    }
    else if (direction == -1)
    {
        startIdx = length - 1;
        endIdx = -1;
    }
    else
    {
        IMPPG_ABORT_MSG("direction must be 1 or -1");
    }

    float prev1, prev2, prev3; // Previously calculated values

    // Assume that border values extend beyond the array
    prev1 = prev2 = prev3 = input[startIdx];

    for (int i = startIdx; i != endIdx; i += direction)
    {
        float next = B * input[i] + (b1*prev1 + b2*prev2 + b3*prev3) * b0inv;
        prev3 = prev2;
        prev2 = prev1;
        prev1 = next;

        output[i] = next;
    }
}

inline void CalculateYvVCoefficients(float sigma, float& b0inv, float& b1, float& b2, float& b3, float& B)
{
    float q;
    if (sigma >= 0.5f && sigma <= 2.5f)
        q = 3.97156f - 4.14554f * sqrtf(1.0f - 0.26891f * sigma);
    else
        q = 0.98711f * sigma - 0.9633f;

    float b0 = 1.57825f + 2.44413f * q + 1.4281f*q*q + 0.422205f*q*q*q;
    b1 = 2.44413f*q + 2.85619f*q*q + 1.26661f*q*q*q;
    b2 = -1.4281f*q*q - 1.26661f*q*q*q;
    b3 = 0.422205f*q*q*q;
    B = 1.0f - ((b1 + b2 + b3) / b0);
    b0inv = 1.0f/b0;
}

void ConvolveGaussianRecursiveTranspose(
    c_PaddedArrayPtr<const float> input,
    c_PaddedArrayPtr<float> output,
    float sigma,
    float tempBuf1[],
    float tempBuf2[]
)
{
    int width = input.width(), height = input.height();
    IMPPG_ASSERT(sigma >= 0.5f);

    float b0inv, b1, b2, b3, B;
    CalculateYvVCoefficients(sigma, b0inv, b1, b2, b3, B);

    float* convRows = tempBuf1;

    // Convolve rows
    #pragma omp parallel for
    for (int y = 0; y < height; y++)
    {
        // Perform forward filtering
        YvVFilterValues(input.row_const(y), &convRows[y*width], width, 1, b0inv, b1, b2, b3, B);

        // Perform backward filtering
        YvVFilterValues(&convRows[y*width], &convRows[y*width], width, -1, b0inv, b1, b2, b3, B);
    }

    float* convRowsT = tempBuf2;
    Transpose<float>(convRows, convRowsT, width, height, width*sizeof(float), height*sizeof(float), TRANSPOSITION_BLOCK_SIZE);

    // Convolve columns (now: rows, since we are using 'convRowsT' as source)
    #pragma omp parallel for
    for (int y = 0; y < width; y++)
    {
        // Perform forward filtering
        YvVFilterValues(&convRowsT[y*height], output.row(y), height, 1, b0inv, b1, b2, b3, B);
        // Perform backward filtering
        YvVFilterValues(output.row(y), output.row(y), height, -1, b0inv, b1, b2, b3, B);
    }
}

void ConvolveSeparableTranspose(
    c_PaddedArrayPtr<const float> input,
    c_PaddedArrayPtr<float> output,
    const float kernel[],
    int kernelRadius,
    float tempBuf1[],
    float tempBuf2[]
)
{
    // NOTE: The function uses only half of 'kernel' (it is symmetrical), but passing the whole array may simplify vectorization in the future.

    int width = input.width(), height = input.height();

    float* convRows = tempBuf1;

    // All zero bits represents 0.0f
    for (int i = 0; i < output.height(); i++)
        memset(output.row(i), 0, output.GetBytesPerRow());

    memset(convRows, 0, width*height*sizeof(float));

    if (width > 2*(kernelRadius-1))
    {
        // Convolve each row
        #pragma omp parallel for
        for (int y = 0; y < height; y++)
        {
            Convolve1Dstep_OfsZero(input.row_const(y) + kernelRadius - 1,
                    convRows + kernelRadius - 1 + y*width,
                    width - 2 * (kernelRadius - 1),
                    kernel[kernelRadius - 1]);

            for (int i = 1; i <= kernelRadius - 1; i++)
            {
                Convolve1Dstep_OfsNonZero(input.row_const(y) + kernelRadius - 1,
                    convRows + kernelRadius - 1 + y*width,
                    width - 2 * (kernelRadius - 1),
                    kernel[i + kernelRadius - 1], i);
            }
        }
    }

    // For near-border elements assume the border values are replicated outside of array
    for (int y = 0; y < height; y++)
    {
        //Left border
        for (int x = -kernelRadius + 1; x <= kernelRadius - 2; x++)
        {
            for (int i = 0; i < kernelRadius; i++)
            {
                float influence = input.row_const(y)[std::max(x - i, 0)] * kernel[i + (kernelRadius - 1)];

                if (x + i >= 0 && x + i < width)
                    convRows[(x + i) + y*width] += influence;
                if (x - i >= 0 && x - i < width && i != 0)
                    convRows[(x - i) + y*width] += influence;
            }
        }

        // Right border
        for (int x = width - (kernelRadius - 1); x <= width + kernelRadius - 1; x++)
        {
            for (int i = 0; i < kernelRadius; i++)
            {
                float influence = input.row_const(y)[std::min(x + i, width - 1)] * kernel[i + (kernelRadius - 1)];
                if (x + i < width && x + i >= 0)
                    convRows[(x + i) + y*width] += influence;
                if (x - i < width && x - i >= 0 && i != 0)
                    convRows[(x - i) + y*width] += influence;
            }
        }
    }

    // Convolve each column

    //For near-border elements assume the border values are replicated outside of array
    for (int x = 0; x < width; x++)
    {
        // Top border
        for (int y = -kernelRadius + 1; y <= kernelRadius - 2; y++)
        {
            for (int i = 0; i < kernelRadius; i++)
            {
                float influence = convRows[x + std::max(y - i, 0) * width] * kernel[i + (kernelRadius - 1)];

                if (y + i >= 0 && y + i < height)
                    output.row(x)[(y + i)] += influence; // 'output' is a transposed array
                if (y - i >= 0 && y - i < height && i != 0)
                    output.row(x)[(y - i)] += influence; // 'output' is a transposed array
            }
        }

        // Bottom border
        for (int y = height - (kernelRadius - 1); y <= height + kernelRadius - 1; y++)
        {
            for (int i = 0; i < kernelRadius; i++)
            {
                float influence = convRows[x + std::min(y + i, height - 1)*width] * kernel[i + (kernelRadius - 1)];
                if (y + i < height && y + i >= 0)
                    output.row(x)[(y + i)] += influence; // 'output' is a transposed array
                if (y - i < height && y - i >= 0 && i != 0)
                    output.row(x)[(y - i)] += influence; // 'output' is a transposed array
            }
        }
    }

    // Before convolving rest of the columns, perform a transposition so we can convolve rows instead (faster due to sequential memory access)

    float* convRowsT = tempBuf2;
    Transpose<float>(convRows, convRowsT, width, height, width * sizeof(float), height * sizeof(float), TRANSPOSITION_BLOCK_SIZE);

    if (height > 2*(kernelRadius-1))
    {
        // Convolve each column (now: row)
        #pragma omp parallel for
        for (int y = 0; y < width; y++)
        {
            Convolve1Dstep_OfsZero(convRowsT + kernelRadius - 1 + y*height,
                output.row(y) + kernelRadius - 1,
                height - 2 * (kernelRadius - 1),
                kernel[kernelRadius - 1]);

            for (int i = 1; i <= kernelRadius - 1; i++)
            {
                Convolve1Dstep_OfsNonZero(convRowsT + kernelRadius - 1 + y*height,
                    output.row(y) + kernelRadius - 1,
                    height - 2 * (kernelRadius - 1),
                    kernel[i + kernelRadius - 1], i);
            }
        }
    }

    // The caller expects a transposed output, so we can leave it as is.
}


void ConvolveSeparable(
    c_PaddedArrayPtr<const float> input,
    c_PaddedArrayPtr<float> output,
    float sigma
)
{
    int width = input.width(), height = input.height();
    int kernelRadius = static_cast<int>(ceil(sigma * 3.0f));

    std::unique_ptr<float[]> outputT(new float[input.height() * input.width()]); // transposed output
    std::unique_ptr<float[]> temp1(new float[input.width() * input.height()]);
    std::unique_ptr<float[]> temp2(new float[input.width() * input.height()]);

    if (kernelRadius < YOUNG_VAN_VLIET_MIN_KERNEL_RADIUS)
    {
        std::unique_ptr<float[]> kernel(new float[2 * kernelRadius - 1]);
        CalculateGaussianKernelProjection(kernel.get(), kernelRadius, sigma, true);

        ConvolveSeparableTranspose(
                input,
                c_PaddedArrayPtr<float>(outputT.get(), height, width),
                kernel.get(), kernelRadius, temp1.get(), temp2.get());
    }
    else
    {
        ConvolveGaussianRecursiveTranspose(
                input,
                c_PaddedArrayPtr<float>(outputT.get(), height, width),
                sigma, temp1.get(), temp2.get());
    }

    Transpose(outputT.get(), output.row(0), height, width, height*sizeof(float), output.GetBytesPerRow(), TRANSPOSITION_BLOCK_SIZE);
}
