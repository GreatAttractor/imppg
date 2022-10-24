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
    Unsharp masking worker thread implementation.
*/

#include "cpu_bmp/lrdeconv.h"
#include "cpu_bmp/w_unshmask.h"

namespace imppg::backend {

c_UnsharpMaskingThread::c_UnsharpMaskingThread(
    WorkerParameters&& params,
    c_View<const IImageBuffer>&& rawInput, ///< Raw/original image fragment without any processing performed
    bool adaptive,   ///< If true, adaptive algorithm is used
    float sigma,     ///< Unsharp mask Gaussian sigma
    float amountMin, ///< Unsharp masking amount min
    float amountMax, ///< Unsharp masking amount max (or just "amount" if 'adaptive' is false)
    float threshold, ///< Brightness threshold for transition from 'amount_min' to 'amount_max'
    float width      ///< Transition width
)
: IWorkerThread(std::move(params)),
  m_RawInput(std::move(rawInput)),
  m_Adaptive(adaptive),
  m_Sigma(sigma),
  m_AmountMin(amountMin),
  m_AmountMax(amountMax),
  m_Threshold(threshold),
  m_Width(width)
{
    IMPPG_ASSERT(m_Params.input.GetWidth() == rawInput.GetWidth());
    IMPPG_ASSERT(m_Params.output.GetWidth() == rawInput.GetWidth());

    IMPPG_ASSERT(m_Params.input.GetHeight() == rawInput.GetHeight());
    IMPPG_ASSERT(m_Params.output.GetHeight() == rawInput.GetHeight());
}

void c_UnsharpMaskingThread::DoWork()
{
    // Width and height of all images (input, raw input, output) are the same
    const unsigned width = m_Params.input.GetWidth();
    const unsigned height = m_Params.input.GetHeight();
    const std::size_t numChannels = NumChannels[static_cast<std::size_t>(m_Params.input.GetPixelFormat())];

    auto gaussianImg = std::make_unique<float[]>(width * height);

    ConvolveSeparable(
        c_PaddedArrayPtr(m_Params.input.GetRowAs<const float>(0), width, height, m_Params.input.GetBytesPerRow()),
        c_PaddedArrayPtr(gaussianImg.get(), width, height),
        m_Sigma,
        numChannels
    );

    if (!m_Adaptive)
    {
        // Standard unsharp masking - the amount (taken from 'm_AmountMax') is constant for the whole image.

        for (int row = 0; row < height; row++)
            for (int col = 0; col < width; col++)
                m_Params.output.GetRowAs<float>(row)[col] =
                    m_AmountMax * m_Params.input.GetRowAs<const float>(row)[col] + (1.0f - m_AmountMax) * gaussianImg[row * width + col];
    }
    else
    {
        // Adaptive unsharp masking - the amount depends on input image's local brightness
        // (henceforth "brightness").
        //
        // Local brightness is taken from the raw, unprocessed image (`m_RawInput`)
        // smoothed by Gaussian with sigma = RAW_IMAGE_BLUR_SIGMA_FOR_ADAPTIVE_UNSHARP_MASK
        // to alleviate noise.
        //
        // See the declaration of `GetAdaptiveUnshMaskTransitionCurve` for further details.

        // gaussian-smoothed raw image to provide the local "steering" brightness
        std::unique_ptr<float[]> imgL(new float[width * height]);
        ConvolveSeparable(
            c_PaddedArrayPtr(m_RawInput.GetRowAs<const float>(0), width, height, m_RawInput.GetBytesPerRow()),
            c_PaddedArrayPtr(imgL.get(), width, height),
            RAW_IMAGE_BLUR_SIGMA_FOR_ADAPTIVE_UNSHARP_MASK,
            numChannels
        );

        const auto& [a, b, c, d] = GetAdaptiveUnshMaskTransitionCurve(m_AmountMin, m_AmountMax, m_Threshold, m_Width);

        for (int row = 0; row < height; row++)
            for (int col = 0; col < width; col++)
            {
                float amount = 1.0f;
                float l = imgL[row * width + col];

                if (l < m_Threshold - m_Width)
                    amount = m_AmountMin;
                else if (l > m_Threshold + m_Width)
                    amount = m_AmountMax;
                else
                    amount = l * (l * (a * l + b) + c) + d;

                m_Params.output.GetRowAs<float>(row)[col] =
                    amount * m_Params.input.GetRowAs<const float>(row)[col] +
                    (1.0f - amount) * gaussianImg[row*m_Params.input.GetWidth() + col];
            }
    }

    Clamp(m_Params.output);
}

} // namespace imppg::backend
