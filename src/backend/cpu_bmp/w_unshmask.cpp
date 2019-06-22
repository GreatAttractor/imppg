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

#include "../../gauss.h"
#include "../../imppg_assert.h"
#include "../../lrdeconv.h"
#include "w_unshmask.h"

namespace imppg::backend {

const float RAW_IMAGE_BLUR_SIGMA = 1.0f;

c_UnsharpMaskingThread::c_UnsharpMaskingThread(
    WorkerParameters&& params,
    c_ImageBufferView&& rawInput, ///< Raw/original image fragment without any processing performed
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
    int width = m_Params.input.GetWidth(), height = m_Params.input.GetHeight();

    auto gaussianImg = std::make_unique<float[]>(width * height);

    ConvolveSeparable(
        c_PaddedArrayPtr(m_Params.input.GetRowAs<float>(0), width, height, m_Params.input.GetBytesPerRow()),
        c_PaddedArrayPtr(gaussianImg.get(), width, height), m_Sigma
    );

    if (!m_Adaptive)
    {
        // Standard unsharp masking - the amount (taken from 'm_AmountMax') is constant for the whole image.

        for (int row = 0; row < height; row++)
            for (int col = 0; col < width; col++)
                m_Params.output.GetRowAs<float>(row)[col] =
                    m_AmountMax * m_Params.input.GetRowAs<float>(row)[col] + (1.0f - m_AmountMax) * gaussianImg[row * width + col];
    }
    else
    {
        // Adaptive unsharp masking - the amount depends on input image's local brightness
        // (henceforth "brightness").
        //
        // Local brightness is taken from the raw, unprocessed image (`m_RawInput`)
        // smoothed by Gaussian with sigma = RAW_IMAGE_BLUR_SIGMA to alleviate noise.
        // The unsharp masking amount is a function of local brightness as follows:
        //
        //   - if brightness < threshold-width, amount is m_AmountMin
        //   - if brightness > threshold+width, amount is m_AmountMax
        //   - if threshold-width <= brightness <= threshold-width (transition region),
        //     amount changes smoothly from m_AmounMin to m_AmountMax following a cubic function:
        //
        //     amount = a*brightness^3 + b*brightness^2 + c*brightness + d
        //
        //  such that its derivatives are zero at (threshold-width) and (threshold+width) and there is
        //  an inflection point at the threshold.


        // gaussian-smoothed raw image to provide the local "steering" brightness
        std::unique_ptr<float[]> imgL(new float[width * height]);
        ConvolveSeparable(
            c_PaddedArrayPtr(m_RawInput.GetRowAs<float>(0), width, height, m_RawInput.GetBytesPerRow()),
            c_PaddedArrayPtr(imgL.get(), width, height),
            RAW_IMAGE_BLUR_SIGMA
        );

        // Coefficients of the cubic curve
        float divisor = 4*m_Width*m_Width*m_Width;
        float a = (m_AmountMin - m_AmountMax) / divisor;
        float b = 3 * (m_AmountMax - m_AmountMin) * m_Threshold / divisor;
        float c = 3 * (m_AmountMax - m_AmountMin) * (m_Width - m_Threshold) * (m_Width + m_Threshold) / divisor;
        float d = (2 * m_Width * m_Width * m_Width * (m_AmountMin + m_AmountMax) +
            3 * m_Threshold * m_Width * m_Width * (m_AmountMin - m_AmountMax) +
            m_Threshold * m_Threshold * m_Threshold * (m_AmountMax - m_AmountMin)) / divisor;

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
                    amount * m_Params.input.GetRowAs<float>(row)[col] +
                    (1.0f - amount) * gaussianImg[row*m_Params.input.GetWidth() + col];
            }
    }

    Clamp(m_Params.output);
}

} // namespace imppg::backend
