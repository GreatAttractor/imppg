/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 201, 2016 Filip Szczerek <ga.software@yahoo.com>

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
    Worker thread implementation.
*/

#include <wx/event.h>
#include "worker.h"
#include "logging.h"

wxThread::ExitCode IWorkerThread::Entry()
{
    Log::Print(wxString::Format("Worker thread (id = %d): started work\n", m_Params.threadId));
    DoWork();
    Log::Print(wxString::Format("Worker thread (id = %d): work finished\n", m_Params.threadId));
    return 0;
}

void IWorkerThread::SendMessageToParent(int messageId, WorkerEventPayload& payload)
{
    auto* event = new wxThreadEvent(wxEVT_THREAD, messageId);
    payload.taskId = m_Params.taskId;
    event->SetPayload(payload);
    event->SetInt(m_Params.threadId);

    // Send the event to the main window (i.e. the main thread)
    m_Params.parent.GetEventHandler()->QueueEvent(event);
}

bool IWorkerThread::IsAbortRequested()
{
    if (TestDestroy() ||
        // Check if the main thread has called Post() on the semaphore
        (wxSEMA_NO_ERROR == abortRequested.TryWait()))
    {
        threadAborted = true;
        return true;
    }
    else
    {
        return false;
    }

}

IWorkerThread::~IWorkerThread()
{
    WorkerEventPayload payload;
    payload.completionStatus = threadAborted ? CompletionStatus::ABORTED : CompletionStatus::COMPLETED;
    SendMessageToParent(ID_FINISHED_PROCESSING, payload);
    {
        auto lock = m_Params.instancePtr.Lock();
        lock.Get() = nullptr;
    }
}

/// Signals the thread to finish processing ASAP
void IWorkerThread::AbortProcessing()
{
    abortRequested.Post();
    threadAborted = true;
}
