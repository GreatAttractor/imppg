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
    Tone curve worker thread class header.
*/

#ifndef IMPPG_TONE_CURVE_WORKER_THREAD_H
#define IMPPG_TONE_CURVE_WORKER_THREAD_H

#include "worker.h"
#include "tcrv.h"

class c_ToneCurveThread: public c_WorkerThread
{
    virtual void DoWork();

    c_ToneCurve toneCurve;
    bool m_UsePreciseValues;

public:
    c_ToneCurveThread(
        wxWindow &parent,             ///< Object to receive notification messages from this worker thread
        wxCriticalSection &guard,     ///< Critical section protecting access to 'instancePtr'
        c_WorkerThread **instancePtr, ///< Pointer to pointer to this thread, set to null when execution finishes
        int taskId,                   ///< Id of task (will be included in every message)
        c_ImageBufferView *input,     ///< Image fragment to process; object will be destroyed when thread ends
        IImageBuffer &output,         ///< Output image
        unsigned threadId,            ///< Unique thread id (not reused by new threads)

        c_ToneCurve &toneCurve,       ///< Tone curve to apply to 'output'; an internal copy will be created
        bool usePreciseValues         ///< If 'false', the approximated curve's values will be used
        );

};

#endif
