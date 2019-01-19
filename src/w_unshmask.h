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
    Unsharp masking worker thread header.
*/

#ifndef IMPPG_UNSHARP_MASKING_WORKER_THREAD_H
#define IMPPG_UNSHARP_MASKING_WORKER_THREAD_H

#include "worker.h"
#include "tcrv.h"

class c_UnsharpMaskingThread: public c_WorkerThread
{
    virtual void DoWork();

    // See comments in c_UnsharpMaskingThread::DoWork() for details
    bool m_Adaptive;
    float m_Sigma;
    float m_AmountMin, m_AmountMax;
    float m_Threshold, m_Width;
    c_ImageBufferView *m_RawInput;

public:
    c_UnsharpMaskingThread(
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
        float threshold,  ///< Brightness threshold for transition from 'amountMin' to 'amountMax'
        float width       ///< Transition width
        );

};

#endif
