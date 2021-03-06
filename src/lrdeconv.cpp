/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2021 Filip Szczerek <ga.software@yahoo.com>

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
    Lucy-Richardson deconvolution implementation.
*/

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <tuple>
#include <vector>

#include "imppg_assert.h"
#include "lrdeconv.h"
#include "gauss.h"


// NOTE: MSVC 18 requires a signed integral type 'for' loop counter
//       when using OpenMP

/// Matrices are transposed in square blocks of this length to a side
const int TRANSPOSITION_BLOCK_SIZE = 16;

/** Minimum radius in pixels (= ceil(3*sigma)) of the Gaussian kernel for which
    the Young & van Vliet recursive convolution is to be used. Below that,
    the standard iterative implementation is currently faster (as tested on my
    Core i5-3570K with DDR3 PC-10700 RAM, compiled with MS C++ 18.00, for 1-4 threads - Filip). */
const int YOUNG_VAN_VLIET_MIN_KERNEL_RADIUS = 8;

/// Transposes matrix 'input' and writes it to 'output'
template<typename T>
void Transpose(const T* input, T* output,
    int width, ///< Number of columns in 'input', rows in 'output'
    int height, ///< Number of rows in 'input', columns in 'output'
    int inputBytesPerRow, int outputBytesPerRow, int blockSize)
{
    // Transpose blocks

    // i, j - indices of the current block
    for (int j = 0; j < height / blockSize; j++)
        for (int i = 0; i < width / blockSize; i++)
        {
            // Addresses of current input and output blocks
            const T* inBlk = reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(input) + (i*sizeof(T) + j*inputBytesPerRow) * blockSize);
            T* outBlk = reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(output) + (j*sizeof(T) + i*outputBytesPerRow) * blockSize);

            // x, y - indices of the current element within the current block
            for (int y = 0; y < blockSize; y++)
                for (int x = 0; x < blockSize; x++)
                {
                    *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(outBlk) + y*sizeof(T) + x*outputBytesPerRow) =
                        *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(inBlk) + x*sizeof(T) + y*inputBytesPerRow);
                }
        }

    // Transpose the remaining elements outside the whole blocks

    // rightmost remaining elements
    for (int j = 0; j < height - (height % blockSize); j++)
        for (int i = width - (width % blockSize); i < width; i++)
        {
            *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(output) + j*sizeof(T) + i*outputBytesPerRow) =
                *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(input) + i*sizeof(T) + j*inputBytesPerRow);
        }

    // bottommost remaining elements
    for (int j = 0; j < width - (width % blockSize); j++)
        for (int i = height - (height % blockSize); i < height; i++)
        {
            *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(output) + i*sizeof(T) + j*outputBytesPerRow) =
                *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(input) + j*sizeof(T) + i*inputBytesPerRow);
        }

    // bottom-right residual rectangle
    for (int j = height - (height % blockSize); j < height; j++)
        for (int i = width - (width % blockSize); i < width; i++)
        {
            *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(output) + j*sizeof(T) + i*outputBytesPerRow) =
                *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(input) + i*sizeof(T) + j*inputBytesPerRow);
        }
}

/// Clamps the values of the specified PIX_MONO32F buffer to [0.0, 1.0]
void Clamp(c_View<IImageBuffer>& buf)
{
    IMPPG_ASSERT(buf.GetPixelFormat() == PixelFormat::PIX_MONO32F);
    for (unsigned j = 0; j < buf.GetHeight(); j++)
    {
        float* row = buf.GetRowAs<float>(j);
        for (unsigned i = 0; i < buf.GetWidth(); i++)
        {
            if (row[i] < 0.0f)
                row[i] = 0.0f;
            else if (row[i] > 1.0f)
                row[i] = 1.0f;
        }
    }
}

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

/// Calculates convolution of 'input' with a rotationally symmetric and separable (i.e. Gaussian) 'kernel' and writes it in transposed form to 'output'
void ConvolveSeparableTranspose(
    c_PaddedArrayPtr<const float> input,  ///< Input array
    c_PaddedArrayPtr<float> output, ///< Transposed output array; contains as many rows as 'input' does columns and as many columns as 'input' does rows
    const float kernel[], ///< Contains convolution kernel's projection (horizontal/vertical); element [kernelRadius] is the middle
    int kernelRadius, ///< 'kernel' contains 2*kernelRadius-1 elements
    float tempBuf1[], ///< Temporary buffer 1, as many elements as 'input'
    float tempBuf2[] ///< Temporary buffer 2, as many elements as 'input'
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

/// Calculates convolution of 'input' with an approximated Gaussian kernel (Young & van Vliet recursive method) and writes it in transposed form to 'output'
void ConvolveGaussianRecursiveTranspose(
    c_PaddedArrayPtr<const float> input,  ///< Input array
    c_PaddedArrayPtr<float> output,       ///< Transposed output array; contains as many rows as 'input' does columns and as many columns as 'input' does rows
    float sigma,                          ///< Gaussian sigma
    float tempBuf1[],                     ///< width*height elements
    float tempBuf2[]                      ///< width*height elements
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

/// Calculates convolution of 'input' with a Gaussian kernel
void ConvolveSeparable(
        c_PaddedArrayPtr<const float> input,  ///< Input array
        c_PaddedArrayPtr<float> output, ///< Output array having as much rows and columns as 'input' does
        float sigma              ///< Gaussian sigma
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

/// Reproduces original image from image in 'input' convolved with Gaussian kernel and writes it to 'output'.
void LucyRichardsonGaussian(
    c_View<const IImageBuffer>& input, ///< Contains a single 'float' value per pixel; size the same as 'output'
    c_View<IImageBuffer>& output, ///< Contains a single 'float' value per pixel; size the same as 'input'
    int numIters,  ///< Number of iterations
    float sigma,   ///< sigma of the Gaussian kernel
    ConvolutionMethod convMethod,

    /// Called after every iteration; arguments: current iteration, total iterations
    std::function<void (int, int)> progressCallback,

    /// Called periodically to check if there was an "abort processing" request
    std::function<bool ()> checkAbort
)
{
    int width = input.GetWidth(), height = input.GetHeight();

    auto prev = std::unique_ptr<float[]>(new float[width * height]);
    auto next = std::unique_ptr<float[]>(new float[width * height]);

    auto inputConvolvedDivT = std::unique_ptr<float[]>(new float[input.GetHeight() * input.GetWidth()]); // a transposed array
    auto estimateConvolvedT = std::unique_ptr<float[]>(new float[input.GetHeight() * input.GetWidth()]); // a transposed array
    auto conv2 = std::unique_ptr<float[]>(new float[input.GetWidth() * input.GetHeight()]);

    auto inputT = std::unique_ptr<float[]>(new float[input.GetHeight() * input.GetWidth()]); // a transposed array

    Transpose(input.GetRowAs<const float>(0), inputT.get(), input.GetWidth(), input.GetHeight(),
        input.GetBytesPerRow(), input.GetHeight() * sizeof(float), TRANSPOSITION_BLOCK_SIZE);

    auto tempBuf1 = std::unique_ptr<float[]>(new float[input.GetWidth() * input.GetHeight()]);
    auto tempBuf2 = std::unique_ptr<float[]>(new float[input.GetWidth() * input.GetHeight()]);

    int kernelRadius = static_cast<int>(ceil(sigma * 3.0f));
    auto kernel = std::unique_ptr<float[]>(new float[2 * kernelRadius - 1]);
    CalculateGaussianKernelProjection(kernel.get(), kernelRadius, sigma, true);

    for (unsigned i = 0; i < input.GetHeight(); i++)
        memcpy(prev.get() + i * input.GetWidth(), input.GetRow(i), input.GetWidth() * sizeof(float));

    for (int i = 0; i < numIters; i++)
    {
        if (convMethod == ConvolutionMethod::STANDARD ||
            convMethod == ConvolutionMethod::AUTO && kernelRadius < YOUNG_VAN_VLIET_MIN_KERNEL_RADIUS)
        {
            ConvolveSeparableTranspose(
                    c_PaddedArrayPtr<const float>(prev.get(), width, height),
                    c_PaddedArrayPtr<float>(estimateConvolvedT.get(), height, width),
                    kernel.get(), kernelRadius, tempBuf1.get(), tempBuf2.get());
        }
        else
            ConvolveGaussianRecursiveTranspose(
                    c_PaddedArrayPtr<const float>(prev.get(), width, height),
                    c_PaddedArrayPtr<float>(estimateConvolvedT.get(), height, width),
                    sigma, tempBuf1.get(), tempBuf2.get());

        #pragma omp parallel for
        for (unsigned j = 0; j < input.GetHeight() * input.GetWidth(); j++)
            inputConvolvedDivT[j] = inputT[j] / (estimateConvolvedT[j] + 1.0e-8f); // add a small epsilon to prevent division by 0 and propagation of NaNs across output pixels

        // Note that 'height' and 'width' are switched in the below calls, as we use transposed arrays for input
        if (convMethod == ConvolutionMethod::STANDARD ||
            convMethod == ConvolutionMethod::AUTO && kernelRadius < YOUNG_VAN_VLIET_MIN_KERNEL_RADIUS)
        {
            ConvolveSeparableTranspose(
                    c_PaddedArrayPtr<const float>(inputConvolvedDivT.get(), height, width),
                    c_PaddedArrayPtr<float>(conv2.get(), width, height),
                    kernel.get(), kernelRadius, tempBuf1.get(), tempBuf2.get());
        }
        else
            ConvolveGaussianRecursiveTranspose(
                    c_PaddedArrayPtr<const float>(inputConvolvedDivT.get(), height, width),
                    c_PaddedArrayPtr<float>(conv2.get(), width, width),
                    sigma, tempBuf1.get(), tempBuf2.get());

        #pragma omp parallel for
        for (unsigned j = 0; j < input.GetWidth() * input.GetHeight(); j++)
            next[j] = prev[j] * conv2[j];

        std::swap(prev, next);

        progressCallback(i, numIters);
        if (checkAbort())
            break;
    }

    for (unsigned i = 0; i < input.GetHeight(); i++)
        memcpy(output.GetRow(i), next.get() + i*input.GetWidth(), input.GetWidth() * sizeof(float));
}

// Functions to encode/decode (x,y) pairs into a 64-bit integer.
inline uint64_t encodeXY(uint32_t x, uint32_t y) { return x + (static_cast<uint64_t>(y) << 32); }
inline std::tuple<uint32_t, uint32_t> decodeXY(uint64_t encoded) { return { encoded & 0xFFFF'FFFF, encoded >> 32 }; }

void FillTresholdVicinityMask(
    c_View<const IImageBuffer> input,
    std::vector<uint8_t>& mask,
    float threshold,
    float sigma
)
{
    IMPPG_ASSERT(input.GetPixelFormat() == PixelFormat::PIX_MONO32F);
    IMPPG_ASSERT(mask.size() == input.GetWidth() * input.GetHeight());

    std::fill(mask.begin(), mask.end(), 0);

    std::vector<uint64_t> borderPixels;

    // Identify all pixels above threshold and create a list of border pixels
    // that are their neighbors, but themselves are below threshold.

    for (int y = 0; y < static_cast<int>(input.GetHeight()); y++)
    {
        for (int x = 0; x < static_cast<int>(input.GetWidth()); x++)
        {
            float valXY = input.GetRowAs<const float>(y)[x];
            if (valXY < threshold)
            {
                for (int j = -1; j <= 1; j++)
                {
                    for (int i = -1; i <= 1; i++)
                    {
                        if (i != 0 && j != 0 &&
                            x + i >= 0 && x + i < static_cast<int>(input.GetWidth()) &&
                            y + j >= 0 && y + j < static_cast<int>(input.GetHeight()))
                        {
                            if (input.GetRowAs<const float>(y + j)[x + i] >= threshold)
                            {
                                borderPixels.push_back(encodeXY(x + i, y + j));
                            }
                        }
                    }
                }
            }
        }
    }

    // Iterate over border pixels, identify all their neighbors to the specified distance
    // and mark both sets in the mask.

    const int influenceDist = static_cast<int>(ceilf(sigma * 2.0f));

    for (const auto bpix: borderPixels)
    {
        const auto [x, y] = decodeXY(bpix);

        for (int i = -(influenceDist - 1); i <= influenceDist - 1; i++)
        {
            for (int j = -(influenceDist - 1); j <= influenceDist - 1; j++)
            {
                if (static_cast<int>(x) + i >= 0 && static_cast<int>(x) + i < static_cast<int>(input.GetWidth()) &&
                    static_cast<int>(y) + j >= 0 && static_cast<int>(y) + j < static_cast<int>(input.GetHeight()))
                {
                    mask[(y + j) * input.GetWidth() + (x + i)] = 1;
                }
            }
        }
    }
}


void BlurThresholdVicinity(
    c_View<const IImageBuffer> input,
    c_View<IImageBuffer> output,
    std::vector<uint8_t>& workBuf,
    float threshold, ///< Threshold to qualify pixels as "border pixels".
    float sigma
)
{
    IMPPG_ASSERT(input.GetWidth() == output.GetWidth() &&
                 input.GetHeight() == output.GetHeight() &&
                 workBuf.size() == input.GetWidth() * input.GetHeight());

    FillTresholdVicinityMask(input, workBuf, threshold, sigma);

    ConvolveSeparable(
        c_PaddedArrayPtr(input.GetRowAs<const float>(0), input.GetWidth(), input.GetHeight(), input.GetBytesPerRow()),
        c_PaddedArrayPtr(output.GetRowAs<float>(0), output.GetWidth(), output.GetHeight(), output.GetBytesPerRow()),
        sigma
    );

    for (unsigned y = 0; y < input.GetHeight(); ++y)
    {
        const float* srcRow = input.GetRowAs<const float>(y);
        const uint8_t* maskRow = &workBuf[y * input.GetWidth()];
        float* destRow = output.GetRowAs<float>(y);

        for (unsigned x = 0; x < input.GetWidth(); ++x)
        {
            destRow[x] = (maskRow[x] == 0) ? srcRow[x] : destRow[x];
        }
    }
}

