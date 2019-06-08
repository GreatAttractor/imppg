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
    Worker thread class header.
*/

#ifndef IMPPG_WORKER_H
#define IMPPG_WORKER_H

#include <atomic>
#include <wx/frame.h>
#include <wx/thread.h>
#include <wx/gdicmn.h>

#include "ctrl_ids.h"
#include "exclusive_access.h"
#include "image.h"

enum class CompletionStatus
{
    COMPLETED = 0,
    ABORTED
};

enum class ProcessingRequest
{
    NONE = 0,
    SHARPENING,
    UNSHARP_MASKING,
    TONE_CURVE
};

// The structure is later encapsulated in wxThreadEvent in a wxAny object,
// so let us keep its size small (below 16 bytes) for better performance
struct WorkerEventPayload
{
    int taskId;
    union
    {
        CompletionStatus completionStatus;
        int percentageComplete;
    };
};

class IWorkerThread;

struct WorkerParameters
{
    wxWindow& parent; ///< Object to receive notification messages from this worker thread.
    ExclusiveAccessObject<IWorkerThread*>& instancePtr; ///< Pointer to this thread, set to null when execution finishes.
    int taskId; ///< Id of task (will be included in every message).
    const c_ImageBufferView input; ///< Image fragment to process.
    c_ImageBufferView output; ///< Output image.
    int threadId; ///< Unique thread id (not reused by new threads).
};

/// Base class representing a worker thread performing processing in the background.
/** Only one instance can be launched at a time. It may spawn more threads
    (not of IWorkerThread class) internally (e.g. via OpenMP) for faster processing. */
class IWorkerThread: public wxThread
{
    std::atomic<bool> threadAborted; ///< 'true' if IsAbortRequested() has been called.

    /// The main thread can perform Post() on this semaphore (via AbortProcessing())
    wxSemaphore abortRequested;

protected:
    WorkerParameters m_Params;

    /// Inherited classes perform processing in this method.
    /** The method should call IsAbortRequested() frequently. */
    virtual void DoWork() = 0;

    bool IsAbortRequested();
    void SendMessageToParent(int messageId, WorkerEventPayload &payload);

public:
    IWorkerThread(WorkerParameters&& params)
    : wxThread(wxTHREAD_DETACHED),
      threadAborted(false),
      abortRequested(0, 1),
      m_Params(std::move(params))
    {}

    ExitCode Entry() override;

    /// Signals the thread to finish processing ASAP
    void AbortProcessing();

    virtual ~IWorkerThread();
};

#endif
