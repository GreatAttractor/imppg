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
    Fast Fourier Transform implementation.
*/

// NOTE: MSVC 18 requires a signed integral type 'for' loop counter
//       when using OpenMP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

#include "common.h"
#include "fft.h"

using std::complex;


const float PI = 3.1415926536f;
const complex<float> I = complex<float>(0, 1);

/// Returns floor(log2(N))
inline unsigned quickLog2(unsigned N)
{
    unsigned result = 0;
    if (N == 0)
        return 0;

    while (N > 0)
    {
        N >>= 1;
        result++;
    }
    return result - 1;
}

/// Calculates 1-dimensional discrete Fourier transform or its inverse (not normalized by N, caller must do this)
template<typename InputT>
void fft1d(
    const InputT* input,  ///< Input vector of length 'N'
    unsigned N,      ///< Number of elements in 'input', has to be a power of two
    complex<float> output[], ///< Output vector of length 'N'
    int stride,       ///< Stride of the input vector in bytes
    int outputStride, ///< Stride of the output vector in bytes

    /// Pointer to the initial twiddle factor for N, i.e. exp(-2*pi*i/N) (or exp(2*pi*i/N) for inverse transform).
    /// NOTE: (twiddlePtr-1) has to point to the lower twiddle factor, i.e. exp(+-2*pi*i/(N/2))
    const complex<float>* twiddlePtr
)
{
    if (N == 1)
        output[0] = input[0];
    else
    {
        fft1d(input, N/2, output, 2*stride, outputStride, twiddlePtr - 1);
        fft1d(reinterpret_cast<const InputT*>(
            reinterpret_cast<const uint8_t*>(input) + stride),
            N/2,
            reinterpret_cast<complex<float>*>(reinterpret_cast<uint8_t*>(output) + N/2*outputStride),
            2*stride,
            outputStride,
            twiddlePtr - 1
        );

        // Initial twiddle factor
        complex<float> tfactor0 = *twiddlePtr;

        complex<float> tfactor = 1.0f;
        for (unsigned k = 0; k <= N/2 - 1; k++)
        {
            complex<float> t = *reinterpret_cast<complex<float>*>(reinterpret_cast<uint8_t*>(output) + k*outputStride);
            complex<float> h = tfactor * (*(reinterpret_cast<complex<float>*>(reinterpret_cast<uint8_t*>(output) + (k + (N>>1))*outputStride)));

            *reinterpret_cast<complex<float>*>(reinterpret_cast<uint8_t*>(output) + k*outputStride) = t + h;
            *reinterpret_cast<complex<float>*>(reinterpret_cast<uint8_t*>(output) + (k + (N>>1))*outputStride) = t - h;

            tfactor *= tfactor0; // in effect, tfactor = exp(-2*PI*I * k/N)
        }
    }
}

void CalcTwiddleFactors(unsigned N, complex<float> table[], bool inverse)
{
    for (int n = quickLog2(N); n >= 0; n--)
    {
        table[n] = (inverse ?
            exp(2.0f * PI * I * (1.0f/N)) :
            exp(-2.0f * PI * I * (1.0f/N)));

        N >>= 1;
    }
}

/// Calculates 2-dimensional discrete Fourier transform
/** Uses the row-column algorithm. */
void CalcFFT2D(
    const float input[], ///< Input array containing rows*cols elements
    unsigned rows, ///< Number of rows, has to be a power of two
    unsigned cols, ///< Number of columns, has to be a power of two
    int stride,    ///< Number of bytes per row in 'input'
    std::complex<float> output[] ///< Output array containing rows*cols elements
)
{
    unsigned maxDim = std::max(rows, cols);

    // Allocate without calling the complex<float> constructor
    std::unique_ptr<complex<float>, BlockDeleter> twiddleFactors(
        static_cast<complex<float>*>(operator new[]((quickLog2(maxDim) + 1) * sizeof(complex<float>)))
    );

    CalcTwiddleFactors(maxDim, twiddleFactors.get(), false);

    // Calculate 1-dimensional transforms of all the rows
    std::unique_ptr<complex<float>, BlockDeleter> fftrows(
        static_cast<complex<float>*>(operator new[](rows * cols * sizeof(complex<float>)))
    );

    #pragma omp parallel for
    for (unsigned k = 0; k < rows; k++)
        fft1d<float>(
            reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(input) + k*stride),
            cols,
            fftrows.get() + k*cols,
            1 * sizeof(float),
            1 * sizeof(complex<float>), twiddleFactors.get() + quickLog2(cols)
        );

    // Calculate 1-dimensional transforms of all columns in 'fftrows' to get the final result
    #pragma omp parallel for
    for (unsigned k = 0; k < cols; k++)
        fft1d<complex<float>>(fftrows.get() + k, rows, output + k, cols * sizeof(complex<float>), cols * sizeof(complex<float>), twiddleFactors.get() + quickLog2(rows));
}

/// Calculates 2-dimensional inverse discrete Fourier transform
/** Uses the row-column algorithm. */
void CalcFFTinv2D(
    const std::complex<float> input[], ///< Input array containing rows*cols elements
    unsigned rows, ///< Number of rows, has to be a power of two
    unsigned cols, ///< Number of columns, has to be a power of two
    std::complex<float> output[] ///< Output array containing rows*cols elements
    )
{
    float rowsInv = 1.0f/rows;
    float colsInv = 1.0f/cols;

    unsigned maxDim = std::max(rows, cols);
    // Allocate without calling the complex<float> constructor
    std::unique_ptr<complex<float>, BlockDeleter> twiddleFactors(
        static_cast<complex<float>*>(operator new[]((quickLog2(maxDim)+1) * sizeof(complex<float>)))
    );

    CalcTwiddleFactors(maxDim, twiddleFactors.get(), true);

    // Calculate 1-dimensional inverse transforms of all the rows
    std::unique_ptr<complex<float>, BlockDeleter> fftrows(
        static_cast<complex<float>*>(operator new[](rows * cols * sizeof(complex<float>)))
    );

    #pragma omp parallel for
    for (unsigned k = 0; k < rows; k++)
        fft1d<complex<float>>(
            input + k*cols,
            cols,
            fftrows.get() + k*cols,
            1 * sizeof(complex<float>),
            1 * sizeof(complex<float>),
            twiddleFactors.get() + quickLog2(cols)
        );


    for (unsigned k = 0; k < rows*cols; k++)
        fftrows.get()[k] *= colsInv;

    // Calculate 1-dimensional inverse transforms of all columns in 'fftrows' to get the final result
    #pragma omp parallel for
    for (unsigned k = 0; k < cols; k++)
        fft1d<complex<float>>(
            fftrows.get() + k,
            rows,
            output + k,
            cols * sizeof(complex<float>),
            cols * sizeof(complex<float>),
            twiddleFactors.get() + quickLog2(rows)
        );

    for (unsigned k = 0; k < rows*cols; k++)
        output[k] *= rowsInv;
}

/// Calculates cross-power spectrum of two 2D discrete Fourier transforms
void CalcCrossPowerSpectrum2D(
    const std::complex<float> F1[], ///< First discrete Fourier transform (N elements)
    const std::complex<float> F2[], ///< Second discrete Fourier transform (N elements)
    std::complex<float> output[], ///< Cross-power spectrum of F1 and F2 (N elements)
    unsigned N ///< Number of array elements
)
{
    #pragma omp parallel for
    for (unsigned i = 0; i < N; i++)
    {
        output[i] = conj(F1[i]) * F2[i];
        float magn = abs(output[i]);
        if (magn > 1.0e-8f)
            output[i] /= magn;
    }
}
