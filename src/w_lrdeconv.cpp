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
    Lucy-Richardson deconvolution worker thread implementation.
*/

#include <wx/datetime.h>
#include "w_lrdeconv.h"
#include "lrdeconv.h"
#include "logging.h"


c_LucyRichardsonThread::c_LucyRichardsonThread(
    wxWindow &parent,             ///< Object to receive notification messages from this worker thread
    wxCriticalSection &guard,     ///< Critical section protecting access to 'instancePtr'
    c_WorkerThread **instancePtr, ///< Pointer to pointer to this thread, set to null when execution finishes
    int taskId,                   ///< Id of task (will be included in every message)
    c_ImageBufferView *input,     ///< Image fragment to process; object will be destroyed when thread ends
    IImageBuffer &output,         ///< Output image
    unsigned threadId,            ///< Unique thread id (not reused by new threads)

    float lrSigma,     ///< Lucy-Richardson deconvolution Gaussian kernel's sigma
    int numIterations, ///< Number of L-R deconvolution iterations
    bool deringing,    ///< If 'true', ringing around a specified threshold of brightness will be reduced
    float deringingThreshold,
    bool deringingGreaterThan,
    float deringingSigma
    )
    :
    c_WorkerThread(parent, guard, instancePtr, taskId, input, output, threadId),
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
    WorkerEventPayload_t payload;
    payload.percentageComplete = 100 * iter / totalIters;
    SendMessageToParent(ID_PROCESSING_PROGRESS, payload);
}

void c_LucyRichardsonThread::DoWork()
{
    wxDateTime tstart = wxDateTime::UNow();

    IImageBuffer *preprocessedInput = input;

    c_Image *preprocessedInputImg = 0;
    if (m_Deringing.enabled)
    {
        preprocessedInputImg = new c_Image(input->GetWidth(), input->GetHeight(), PIX_MONO32F);
        BlurThresholdVicinity(*input, preprocessedInputImg->GetBuffer(), m_Deringing.threshold, m_Deringing.greaterThan, m_Deringing.sigma);
        preprocessedInput = &preprocessedInputImg->GetBuffer();
    }

    LucyRichardsonGaussian(*preprocessedInput, output, numIterations, lrSigma, CONV_AUTO,
        [this](int currentIter, int totalIters) { IterationNotification(currentIter, totalIters); },
        [this]() { return IsAbortRequested(); }
    );
    Log::Print(wxString::Format("L-R deconvolution finished in %s s\n", (wxDateTime::UNow() - tstart).Format("%S.%l")));
    Clamp((float *)output.GetRow(0), output.GetWidth(), output.GetHeight(), output.GetBytesPerRow());
    
    if (preprocessedInputImg)
        delete preprocessedInputImg;
}
