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

#include "cpu_bmp/worker.h"

#include <optional>

namespace imppg::backend {

class c_UnsharpMaskingThread: public IWorkerThread
{
    virtual void DoWork();

    std::optional<c_View<const IImageBuffer>> m_BlurredRawInput; ///< Raw/original image fragment smoothed to alleviate noise.
    UnsharpMask m_UnsharpMask;

public:
    c_UnsharpMaskingThread(
        WorkerParameters&& params,
        std::optional<c_View<const IImageBuffer>>&& m_BlurredRawInput,
        UnsharpMask unsharpMask
    );
};

} // namespace imppg::backend

#endif
