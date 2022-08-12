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
    Lucy-Richardson deconvolution worker thread class header.
*/

#ifndef IMPPG_LR_DECONV_WORKER_THREAD_H
#define IMPPG_LR_DECONV_WORKER_THREAD_H

#include "cpu_bmp/worker.h"

namespace imppg::backend {

class c_LucyRichardsonThread : public IWorkerThread
{
    void DoWork() override;

    float lrSigma;
    int numIterations;
    struct
    {
        bool enabled;
        float threshold;
        float sigma;
        std::vector<uint8_t>& workBuf; ///< Must have as many elements as there are input pixels.
    } m_Deringing;

    void IterationNotification(int iter, int totalIters);

public:
    c_LucyRichardsonThread(
        WorkerParameters&& params,
        float lrSigma,             ///< Lucy-Richardson deconvolution Gaussian kernel's sigma.
        int numIterations,         ///< Number of L-R deconvolution iterations.
        bool deringing,            ///< If 'true', ringing around a specified threshold of brightness will be reduced.
        float deringingThreshold,
        float deringingSigma,
        std::vector<uint8_t>& deringingWorkBuf ///< Must have as many elements as there are input pixels.
    );
};

} // namespace imppg::backend

#endif
