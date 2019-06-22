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
#include "w_lrdeconv.h"
#include "../../lrdeconv.h"
#include "../../ctrl_ids.h"
#include "../../logging.h"

namespace imppg::backend {

c_LucyRichardsonThread::c_LucyRichardsonThread(
    WorkerParameters&& params,
    float lrSigma,     ///< Lucy-Richardson deconvolution Gaussian kernel's sigma
    int numIterations, ///< Number of L-R deconvolution iterations
    bool deringing,    ///< If 'true', ringing around a specified threshold of brightness will be reduced
    float deringingThreshold,
    bool deringingGreaterThan,
    float deringingSigma
): IWorkerThread(std::move(params)),
   lrSigma(lrSigma),
   numIterations(numIterations)
{
    m_Deringing.enabled = deringing;
    m_Deringing.threshold = deringingThreshold;
    m_Deringing.greaterThan = deringingGreaterThan;
    m_Deringing.sigma = deringingSigma;
}

void c_LucyRichardsonThread::IterationNotification(int iter, int totalIters)
{
    WorkerEventPayload payload;
    payload.percentageComplete = 100 * iter / totalIters;
    SendMessageToParent(ID_PROCESSING_PROGRESS, payload);
}

void c_LucyRichardsonThread::DoWork()
{
    wxDateTime tstart = wxDateTime::UNow();

    c_ImageBufferView preprocessedInput = m_Params.input;

    std::unique_ptr<c_Image> preprocessedInputImg;
    if (m_Deringing.enabled)
    {
        preprocessedInputImg = std::make_unique<c_Image>(m_Params.input.GetWidth(), m_Params.input.GetHeight(), PixelFormat::PIX_MONO32F);
        auto preprocView = c_ImageBufferView(preprocessedInputImg->GetBuffer());
        BlurThresholdVicinity(m_Params.input, preprocView, m_Deringing.threshold, m_Deringing.greaterThan, m_Deringing.sigma);
        preprocessedInput = c_ImageBufferView(preprocessedInputImg->GetBuffer());
    }

    LucyRichardsonGaussian(preprocessedInput, m_Params.output, numIterations, lrSigma, ConvolutionMethod::AUTO,
        [this](int currentIter, int totalIters) { IterationNotification(currentIter, totalIters); },
        [this]() { return IsAbortRequested(); }
    );
    Log::Print(wxString::Format("L-R deconvolution finished in %s s\n", (wxDateTime::UNow() - tstart).Format("%S.%l")));
    Clamp(m_Params.output);
}

} // namespace imppg::backend
