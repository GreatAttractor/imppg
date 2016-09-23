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
    Gaussian kernel calculations header.
*/

#ifndef IMPPG_GAUSSIAN_H
#define IMPPG_GAUSSIAN_H

/// Calculates a (quarter of) 2D Gaussian kernel; element [0] is the peak (middle)
void CalculateGaussianKernel(
        float kernel[], ///< Quarter of 2D kernel; element [0] is the middle (peak)
        int length,     ///< 'kernel' contains length*length elements
        float sigma,    ///< Gaussian sigma
        bool normalize);

/// Calculates a 1D projection of a 2D Gaussian kernel
void CalculateGaussianKernelProjection(
        float kernel[], ///< Kernel values; element [radius-1] is the 1D kernel's middle
        int radius, ///< Should be at least 3*sigma; 'kernel' contains 2*radius-1 elements
        float sigma, ///< Gaussian sigma
        bool normalized
);

/// Applies Gaussian blur to the specified vector
void GaussianBlur1D(
    float values[], ///< Float vector to blur
    int numValues, ///< Number of elements in 'values'
    float sigma ///< Gaussian sigma
);

#endif // IMPPG_GAUSSIAN_H
