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
    Tone curve worker thread class header.
*/

#ifndef IMPPG_TONE_CURVE_WORKER_THREAD_H
#define IMPPG_TONE_CURVE_WORKER_THREAD_H

#include "cpu_bmp/worker.h"
#include "common/tcrv.h"

namespace imppg::backend {

class c_ToneCurveThread: public IWorkerThread
{
    void DoWork() override;

    c_ToneCurve toneCurve;
    bool m_UsePreciseValues;

public:
    c_ToneCurveThread(
        WorkerParameters&& params,
        const c_ToneCurve &toneCurve,   ///< Tone curve to apply to 'output'; an internal copy will be created
        bool usePreciseValues           ///< If 'false', the approximated curve's values will be used
    );

};

} // namespace imppg::backend

#endif
