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
    Gaussian kernel calculations.
*/

#include <math.h>
#include <memory>
#include <cstring>
#include <algorithm>
#include "gauss.h"
#include "common.h"

/// Normalizes a quarter of a rotationally symmetric 2D kernel
void NormalizeKernel(float kernel[], int radius)
{
    int i, j;
    float sum = 0.0f;

    sum += kernel[0 + 0*radius];

    // All elements along the main axes contribute twice to the full kernel's sum of elements
    for (i = 1; i < radius; i++)
        sum += 2 * (kernel[i + 0*radius] + kernel[0 + i*radius]);

    // All elements inside the quarter contribute 4 times to the full kernel's sum of elements
    for (i = 1; i < radius; i++)
        for (j = 1; j < radius; j++)
            sum += 4 * (kernel[i + j*radius]);

    for (i = 0; i < radius*radius; i++)
        kernel[i] /= sum;
}

/// Calculates (a quarter of) normalized 2D Gaussian kernel; element [0] is the peak (middle)
void CalculateGaussianKernel(
        float kernel[], ///< Quarter of 2D kernel; element [0] is the middle (peak)
        int length,     ///< 'kernel' contains length*length elements
        float sigma,    ///< Gaussian sigma
        bool normalize)
{
    for (int i = 0; i < length; i++)
        for (int j = 0; j < length; j++)
            kernel[i + j*length] = expf(-(SQR(i) + SQR(j)) / (2*SQR(sigma)));

    if (normalize)
        NormalizeKernel(kernel, length);
}

/// Calculates a 1D projection of a 2D Gaussian kernel
void CalculateGaussianKernelProjection(
        float kernel[], ///< Kernel values; element [radius-1] is the 1D kernel's middle
        int radius, ///< Should be at least 3*sigma; 'kernel' contains 2*radius-1 elements
        float sigma, ///< Gaussian sigma
        bool normalized
)
{
    int i;
    for (i = 0; i < 2*radius-1; i++)
    {
        kernel[i] = expf(-SQR((radius-1) - i) / (2.0f * sigma * sigma));
    }

    if (normalized)
    {
        float sum = 0.0f;
        for (i = 0; i < 2 * radius - 1; i++)
            sum += kernel[i];

        for (i = 0; i < 2 * radius - 1; i++)
            kernel[i] /= sum;
    }
}

/// Applies Gaussian blur to the specified vector
void GaussianBlur1D(
    float values[], ///< Float vector to blur
    int numValues, ///< Number of elements in 'values'
    float sigma ///< Gaussian sigma
    )
{
    int radius = static_cast<int>(ceilf(3*sigma));
    std::unique_ptr<float[]> kernel(new float[2*radius-1]);
    CalculateGaussianKernelProjection(kernel.get(), radius, sigma, true);

    std::unique_ptr<float[]> result(new float[numValues]);
    memset(result.get(), 0, numValues * sizeof(float));

    for (int i = 0; i < numValues; i++)
        for (int j = 0; j < 2*radius-1; j++)
        {
            int influenceSrcIdx = i - (radius-1) + j;
            if (influenceSrcIdx < 0)
                influenceSrcIdx = 0;
            else if (influenceSrcIdx >= numValues)
                influenceSrcIdx = numValues - 1;

            float influenceSrc = values[influenceSrcIdx];
            result[i] += influenceSrc * kernel[j];
        }

    std::memcpy(values, result.get(), numValues * sizeof(float));
}
