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
    Tone curve worker thread implementation.
*/

#include <wx/datetime.h>
#include "w_tcurve.h"
#include "logging.h"


c_ToneCurveThread::c_ToneCurveThread(
    wxWindow &parent,             ///< Object to receive notification messages from this worker thread
    wxCriticalSection &guard,     ///< Critical section protecting access to 'instancePtr'
    c_WorkerThread **instancePtr, ///< Pointer to pointer to this thread, set to null when execution finishes
    int taskId,                   ///< Id of task (will be included in every message)
    c_ImageBufferView *input,     ///< Image fragment to process; object will be destroyed when thread ends
    IImageBuffer &output,         ///< Output image
    unsigned threadId,            ///< Unique thread id (not reused by new threads)

    c_ToneCurve &toneCurve, ///< Tone curve to apply to 'output'; an internal copy will be created
    bool usePreciseValues ///< If 'false', the approximated curve's values will be used
)
: c_WorkerThread(parent, guard, instancePtr, taskId, input, output, threadId), toneCurve(toneCurve), m_UsePreciseValues(usePreciseValues)
{
}

void c_ToneCurveThread::DoWork()
{
    wxDateTime tstart = wxDateTime::UNow();
    toneCurve.RefreshLut();

    float (c_ToneCurve::* valueGetter)(float);
    if (m_UsePreciseValues)
        valueGetter = &c_ToneCurve::GetPreciseValue;
    else
        valueGetter = &c_ToneCurve::GetApproximatedValue;

    int lastPercentageReported = 0;
    for (unsigned y = 0; y < output.GetHeight(); y++)
    {
        for (unsigned x = 0; x < output.GetWidth(); x++)
            ((float *)output.GetRow(y))[x] = (toneCurve.*valueGetter)(((float *)input->GetRow(y))[x]);

        // Notify the main thread after every 5% of progress
        int percentage = 100 * y / output.GetHeight();
        if (percentage > lastPercentageReported + 5)
        {
            WorkerEventPayload_t payload;
            payload.percentageComplete = percentage;
            SendMessageToParent(ID_PROCESSING_PROGRESS, payload);
            lastPercentageReported = percentage;
        }

        if (IsAbortRequested())
            break;
    }
    Log::Print(wxString::Format("Applying of tone curve finished in %s s\n", (wxDateTime::UNow() - tstart).Format("%S.%l")));
}
