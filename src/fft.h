/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2017 Filip Szczerek <ga.software@yahoo.com>

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
    Fast Fourier Transform functions header.
*/
#ifndef IMPPG_FFT_HEADER
#define IMPPG_FFT_HEADER

#include <complex>

/// Calculates 2-dimensional discrete Fourier transform
/** Uses the row-column algorithm. */
void CalcFFT2D(
    float input[], ///< Input array containing rows*cols elements
    unsigned rows, ///< Number of rows, has to be a power of two
    unsigned cols, ///< Number of columns, has to be a power of two
    int stride,    ///< Number of bytes per row in 'input'
    std::complex<float> output[] ///< Output array containing rows*cols elements
);

/// Calculates 2-dimensional inverse discrete Fourier transform
/** Uses the row-column algorithm. */
void CalcFFTinv2D(
    std::complex<float> input[], ///< Input array containing rows*cols elements
    unsigned rows, ///< Number of rows, has to be a power of two
    unsigned cols, ///< Number of columns, has to be a power of two
    std::complex<float> output[] ///< Output array containing rows*cols elements
);

/// Calculates cross-power spectrum of two 2D discrete Fourier transforms
void CalcCrossPowerSpectrum2D(
    std::complex<float> F1[], ///< First discrete Fourier transform (N elements)
    std::complex<float> F2[], ///< Second discrete Fourier transform (N elements)
    std::complex<float> output[], ///< Cross-power spectrum of F1 and F2 (N elements)
    unsigned N ///< Number of array elements
);

#endif // IMPPG_FFT_HEADER
