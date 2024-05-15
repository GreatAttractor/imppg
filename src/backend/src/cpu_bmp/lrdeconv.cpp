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

#include "lrdeconv.h"
#include "math_utils/gauss.h"


// NOTE: MSVC 18 requires a signed integral type 'for' loop counter
//       when using OpenMP

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
    IMPPG_ASSERT(input.GetPixelFormat() == PixelFormat::PIX_MONO32F);
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
    IMPPG_ASSERT(input.GetWidth() == output.GetWidth());
    IMPPG_ASSERT(input.GetHeight() == output.GetHeight());
    IMPPG_ASSERT(workBuf.size() == input.GetWidth() * input.GetHeight());
    IMPPG_ASSERT(input.GetPixelFormat() == PixelFormat::PIX_MONO32F);
    IMPPG_ASSERT(input.GetPixelFormat() == output.GetPixelFormat());

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
