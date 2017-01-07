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
    Unsharp masking worker thread implementation.
*/

#include <cassert>
#include "w_unshmask.h"
#include "lrdeconv.h"
#include "gauss.h"

const float RAW_IMAGE_BLUR_SIGMA = 1.0f;

c_UnsharpMaskingThread::c_UnsharpMaskingThread(
    wxWindow &parent,             ///< Object to receive notification messages from this worker thread
    wxCriticalSection &guard,     ///< Critical section protecting access to 'instancePtr'
    c_WorkerThread **instancePtr, ///< Pointer to pointer to this thread, set to null when execution finishes
    int taskId,                   ///< Id of task (will be included in every message)
    c_ImageBufferView *input,     ///< Image fragment to process; object will be destroyed when thread ends
    c_ImageBufferView *rawInput,  ///< Raw/original image fragment without any processing performed
    IImageBuffer &output,         ///< Output image
    unsigned threadId,            ///< Unique thread id (not reused by new threads)

    bool adaptive, ///< If true, adaptive algorithm is used
    float sigma,  ///< Unsharp mask Gaussian sigma
    float amountMin, ///< Unsharp masking amount min
    float amountMax, ///< Unsharp masking amount max (or just "amount" if 'adaptive' is false)
    float threshold,  ///< Brightness threshold for transition from 'amount_min' to 'amount_max'
    float width       ///< Transition width
)
: c_WorkerThread(parent, guard, instancePtr, taskId, input, output, threadId),
  m_Adaptive(adaptive), m_Sigma(sigma), m_AmountMin(amountMin), m_AmountMax(amountMax), m_Threshold(threshold), m_Width(width), m_RawInput(rawInput)
{
    assert(input->GetWidth() == rawInput->GetWidth());
    assert(rawInput->GetWidth() == output.GetWidth());

    assert(input->GetHeight() == rawInput->GetHeight());
    assert(rawInput->GetHeight() == output.GetHeight());
}

void c_UnsharpMaskingThread::DoWork()
{
    // Width and height of all images (input, raw input, output) are the same
    int width = input->GetWidth(), height = input->GetHeight();

    std::unique_ptr<float[]> gaussianImg(new float[width * height]);

    ConvolveSeparable(
            c_PaddedArrayPtr<float>((float *)input->GetRow(0), width, height, input->GetBytesPerRow()),
            c_PaddedArrayPtr<float>(gaussianImg.get(), width, height), m_Sigma);

    if (!m_Adaptive)
    {
        // Standard unsharp masking - the amount (taken from 'm_AmountMax') is constant for the whole image.

        for (int row = 0; row < height; row++)
            for (int col = 0; col < width; col++)
                ((float *)output.GetRow(row))[col] = m_AmountMax * ((float *)input->GetRow(row))[col] + (1.0f - m_AmountMax) * gaussianImg[row * width + col];
    }
    else
    {
    /*
        Adaptive unsharp masking - the amount depends on input image's local brightness (henceforth "brightness").

        Local brightness is taken from the raw, unprocessed image ('m_RawInput') smoothed by Gaussian with sigma = RAW_IMAGE_BLUR_SIGMA to alleviate noise.
        The unsharp masking amount is a function of local brightness as follows:

                - if brightness < threshold-width, amount is m_AmountMin
                - if brightness > threshold+width, amount is m_AmountMax
                - if threshold-width <= brightness <= threshold-width (transition region), amount changes smoothly from m_AmounMin to m_AmountMax following a cubic function:

                    amount = a*brightness^3 + b*brightness^2 + c*brightness + d

                such that its derivatives are zero at (threshold-width) and (threshold+width) and there is an inflection point at the threshold.
    */

        // Gaussian-smoothed raw image to provide the local "steering" brightness
        std::unique_ptr<float[]> imgL(new float[width * height]);
        ConvolveSeparable(
                c_PaddedArrayPtr<float>((float *)m_RawInput->GetRow(0), width, height, m_RawInput->GetBytesPerRow()),
                c_PaddedArrayPtr<float>(imgL.get(), width, height),
                RAW_IMAGE_BLUR_SIGMA);

        // Coefficients of the cubic curve
        float divisor = 4*m_Width*m_Width*m_Width;
        float a = (m_AmountMin - m_AmountMax) / divisor;
        float b = 3 * (m_AmountMax - m_AmountMin) * m_Threshold / divisor;
        float c = 3 * (m_AmountMax - m_AmountMin) * (m_Width - m_Threshold) * (m_Width + m_Threshold) / divisor;
        float d = (2 * m_Width * m_Width * m_Width * (m_AmountMin + m_AmountMax) + 3 * m_Threshold * m_Width * m_Width * (m_AmountMin - m_AmountMax)
                + m_Threshold * m_Threshold * m_Threshold * (m_AmountMax - m_AmountMin)) / divisor;


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

                ((float *)output.GetRow(row))[col] = amount * ((float *)input->GetRow(row))[col] + (1.0f - amount) * gaussianImg[row*input->GetWidth() + col];
            }
    }

    Clamp((float *)output.GetRow(0), output.GetWidth(), output.GetHeight(), output.GetBytesPerRow());
}
