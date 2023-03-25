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
    std::optional<c_View<const IImageBuffer>>&& blurredRawInput,
    UnsharpMask unsharpMask
)
: IWorkerThread(std::move(params)),
  m_BlurredRawInput(std::move(blurredRawInput)),
  m_UnsharpMask(unsharpMask)
{
    if (m_BlurredRawInput.has_value())
    {
        IMPPG_ASSERT(m_BlurredRawInput->GetPixelFormat() == PixelFormat::PIX_MONO32F);
    }
}

void c_UnsharpMaskingThread::DoWork()
{
    // width and height of all images (input, raw input, output) are the same
    const unsigned width = m_Params.input.at(0).GetWidth();
    const unsigned height = m_Params.input.at(0).GetHeight();

    std::vector<std::unique_ptr<float[]>> gaussianImg;

    for (std::size_t ch = 0; ch < m_Params.input.size(); ++ch)
    {
        gaussianImg.emplace_back(std::make_unique<float[]>(width * height));
        ConvolveSeparable(
            c_PaddedArrayPtr(m_Params.input.at(ch).GetRowAs<const float>(0), width, height, m_Params.input.at(ch).GetBytesPerRow()),
            c_PaddedArrayPtr(gaussianImg.at(ch).get(), width, height), m_UnsharpMask.sigma
        );
    }

    if (!m_UnsharpMask.adaptive)
    {
        // Standard unsharp masking - the amount (taken from `m_AmountMax`) is constant for the whole image.

        for (std::size_t ch = 0; ch < m_Params.input.size(); ++ch)
        {
            for (unsigned row = 0; row < height; row++)
            {
                const float* srcRow = m_Params.input.at(ch).GetRowAs<const float>(row);
                const float* gaussianRow = &gaussianImg.at(ch)[row * width];
                float* destRow = m_Params.output.at(ch).GetRowAs<float>(row);

                for (unsigned col = 0; col < width; col++)
                {
                    destRow[col] = m_UnsharpMask.amountMax * srcRow[col] + (1.0f - m_UnsharpMask.amountMax) * gaussianRow[col];
                }
            }
        }
    }
    else
    {
        // Adaptive unsharp masking - the amount depends on input image's local brightness. It is taken from the raw,
        // unprocessed image smoothed by Gaussian with sigma = RAW_IMAGE_BLUR_SIGMA_FOR_ADAPTIVE_UNSHARP_MASK
        // to alleviate noise (`m_BlurredRawInput`). See the declaration of `GetAdaptiveUnshMaskTransitionCurve`
        // for further details.

        const auto [a, b, c, d] = GetAdaptiveUnshMaskTransitionCurve(m_UnsharpMask);

        const float amountMin = m_UnsharpMask.amountMin;
        const float amountMax = m_UnsharpMask.amountMax;
        const float threshold = m_UnsharpMask.threshold;
        const float transitionWidth = m_UnsharpMask.width;

        for (std::size_t channel = 0; channel < m_Params.input.size(); ++channel)
        {
            for (unsigned row = 0; row < height; row++)
            {
                const float* srcRow = m_Params.input.at(channel).GetRowAs<const float>(row);
                const float* lumRow = m_BlurredRawInput.value().GetRowAs<const float>(row);
                const float* gaussianRow = &gaussianImg[channel][row * m_Params.input.at(0).GetWidth()];
                float* destRow = m_Params.output.at(channel).GetRowAs<float>(row);

                for (unsigned col = 0; col < width; col++)
                {
                    const float amount = [&, a = a, b = b, c = c, d = d]() {
                        const float lum = lumRow[col];
                        if (lum < threshold - transitionWidth)
                        {
                            return amountMin;
                        }
                        else if (lum > threshold + transitionWidth)
                        {
                            return amountMax;
                        }
                        else
                        {
                            return lum * (lum * (a * lum + b) + c) + d;
                        }
                    }();

                    destRow[col] = amount * srcRow[col] + (1.0f - amount) * gaussianRow[col];
                }

                if (row % 256 == 0)
                {
                    if (IsAbortRequested())
                        break;
                }
            }
        }
    }

    for (auto& channel: m_Params.output)
    {
        Clamp(channel);
    }
}

} // namespace imppg::backend
