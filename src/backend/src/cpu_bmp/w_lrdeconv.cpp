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
    Lucy-Richardson deconvolution worker thread implementation.
*/

#include <wx/datetime.h>
#include "cpu_bmp/w_lrdeconv.h"
#include "lrdeconv.h"
#include "cpu_bmp/message_ids.h"
#include "logging/logging.h"

namespace imppg::backend {

c_LucyRichardsonThread::c_LucyRichardsonThread(
    WorkerParameters&& params,
    float lrSigma,
    int numIterations,
    bool deringing,
    float deringingThreshold,
    float deringingSigma,
    std::vector<uint8_t>& deringingWorkBuf
): IWorkerThread(std::move(params)),
   lrSigma(lrSigma),
   numIterations(numIterations),
   m_Deringing{deringing, deringingThreshold, deringingSigma, deringingWorkBuf}
{
}

void c_LucyRichardsonThread::IterationNotification(int iter, int totalIters)
{
    const int percentage = 100 * iter / totalIters;

    if (percentage >= m_LastReportedPercentage + 5)
    {
        WorkerEventPayload payload;
        payload.percentageComplete = percentage;
        SendMessageToParent(ID_PROCESSING_PROGRESS, payload);
        m_LastReportedPercentage = percentage;
    }
}

void c_LucyRichardsonThread::DoWork()
{
    wxDateTime tstart = wxDateTime::UNow();

    std::vector<c_View<const IImageBuffer>> preprocessedInput;
    for (const auto& input: m_Params.input)
    {
        preprocessedInput.emplace_back(input);
    }

    const std::size_t numChannels = m_Params.input.size();

    std::vector<c_Image> preprocessedInputImg;
    if (m_Deringing.enabled)
    {
        for (const auto& input: m_Params.input)
        {
            preprocessedInputImg.emplace_back(input.GetWidth(), input.GetHeight(), PixelFormat::PIX_MONO32F);
        }

        for (std::size_t ch = 0; ch < numChannels; ++ch)
        {
            auto preprocView = c_View(preprocessedInputImg.at(ch).GetBuffer());
            BlurThresholdVicinity(m_Params.input.at(ch), preprocView, m_Deringing.workBuf, m_Deringing.threshold, m_Deringing.sigma);
            preprocessedInput[ch] = c_View<const IImageBuffer>(preprocessedInputImg.at(ch).GetBuffer());
        }
    }

    for (std::size_t ch = 0; ch < numChannels; ++ch)
    {
        LucyRichardsonGaussian(preprocessedInput.at(ch), m_Params.output.at(ch), numIterations, lrSigma, ConvolutionMethod::AUTO,
            [this, ch, numChannels](int currentIter, int totalIters) {
                IterationNotification(ch * totalIters + currentIter, totalIters * numChannels);
            },
            [this]() { return IsAbortRequested(); }
        );
    }

    Log::Print(wxString::Format("L-R deconvolution finished in %s s\n", (wxDateTime::UNow() - tstart).Format("%S.%l")));
    Clamp(m_Params.output.at(0));
}

} // namespace imppg::backend
