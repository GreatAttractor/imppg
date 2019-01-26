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
    Worker thread class header.
*/

#ifndef IMPPG_WORKER_H
#define IMPPG_WORKER_H

#include <wx/frame.h>
#include <wx/thread.h>
#include <wx/gdicmn.h>
#include "image.h"
#include "ctrl_ids.h"

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
typedef struct
{
    int taskId;
    union
    {
        CompletionStatus completionStatus;
        int percentageComplete;
    };
} WorkerEventPayload_t;

/// Base class representing a worker thread performing processing in the background.
/** Only one instance can be launched at a time. It may spawn more threads
    (not of c_WorkerThread class) internally (e.g. via OpenMP) for faster processing. */
class c_WorkerThread: public wxThread
{
    wxWindow &parent;
    wxCriticalSection &guard;
    c_WorkerThread **instancePtr;
    bool threadAborted; ///< 'true' if IsAbortRequested() has been called and has returned 'true'
    int taskId; ///< Id of the task (included in every message's payload)
    unsigned threadId; ///< Unique thread id (not reused by new threads)

    /// The main thread can perform Post() on this semaphore (via AbortProcessing())
    wxSemaphore abortRequested;

protected:
    /// Inherited classes perform processing in this method
    /** The method should call IsAbortRequested() frequently. */
    virtual void DoWork() = 0;

    c_ImageBufferView *input;
    IImageBuffer &output;
    bool IsAbortRequested();
    void SendMessageToParent(int messageId, WorkerEventPayload_t &payload);

public:
    c_WorkerThread(wxWindow &parent,  ///< Object to receive notification messages from this worker thread
        wxCriticalSection &guard,     ///< Critical section protecting access to 'instancePtr'
        c_WorkerThread **instancePtr, ///< Pointer to pointer to this thread, set to null when execution finishes
        int taskId,                   ///< Id of task (will be included in every message)
        c_ImageBufferView *input,     ///< Image fragment to process; object will be destroyed when thread ends
        IImageBuffer &output,         ///< Output image
        unsigned threadId             ///< Unique thread id (not reused by new threads)
        )
        : wxThread(wxTHREAD_DETACHED),
        parent(parent),
        guard(guard),
        instancePtr(instancePtr),
        threadAborted(false),
        taskId(taskId),
        threadId(threadId),
        abortRequested(0, 1),
        input(input),
        output(output)
    { }

    virtual ExitCode Entry();

    /// Signals the thread to finish processing ASAP
    void AbortProcessing();

    virtual ~c_WorkerThread();
};

#endif
