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
    Tone curve worker thread implementation.
*/

#include <wx/datetime.h>

#include "cpu_bmp/w_tcurve.h"
#include "logging/logging.h"
#include "message_ids.h"

namespace imppg::backend {

c_ToneCurveThread::c_ToneCurveThread(
    WorkerParameters&& params,
    const c_ToneCurve& toneCurve,   ///< Tone curve to apply to 'output'; an internal copy will be created
    bool usePreciseValues           ///< If 'false', the approximated curve's values will be used
): IWorkerThread(std::move(params)),
   toneCurve(toneCurve),
   m_UsePreciseValues(usePreciseValues)
{}

void c_ToneCurveThread::DoWork()
{
    wxDateTime tstart = wxDateTime::UNow();
    toneCurve.RefreshLut();

    const std::size_t numChannels = m_Params.input.size();

    const unsigned width = m_Params.output.at(0).GetWidth();
    const unsigned height = m_Params.output.at(0).GetHeight();

    int lastPercentageReported = 0;
    for (std::size_t ch = 0; ch < numChannels; ++ch)
    {
        for (unsigned y = 0; y < height; y++)
        {
            if (m_UsePreciseValues)
            {
                toneCurve.ApplyPreciseToneCurve(
                    m_Params.input.at(ch).GetRowAs<const float>(y),
                    m_Params.output.at(ch).GetRowAs<float>(y),
                    width
                );
            }
            else
            {
                toneCurve.ApplyApproximatedToneCurve(
                    m_Params.input.at(ch).GetRowAs<const float>(y),
                    m_Params.output.at(ch).GetRowAs<float>(y),
                    width
                );
            }

            // Notify the main thread after every 5% of progress
            int percentage = 100 * (y + ch * height) / (numChannels * height);
            if (percentage > lastPercentageReported + 5)
            {
                WorkerEventPayload payload;
                payload.percentageComplete = percentage;
                SendMessageToParent(ID_PROCESSING_PROGRESS, payload);
                lastPercentageReported = percentage;
            }

            if (IsAbortRequested())
                break;
        }
    }
    Log::Print(wxString::Format("Applying of tone curve finished in %s s\n", (wxDateTime::UNow() - tstart).Format("%S.%l")));
}

} // namespace imppg::backend
