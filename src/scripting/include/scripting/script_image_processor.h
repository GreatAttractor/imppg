/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2022-2025 Filip Szczerek <ga.software@yahoo.com>

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
    Script image processor header.
*/

#pragma once

#include "backend/backend.h"
#include "scripting/interop.h"

#include <functional>
#include <memory>
#include <optional>
#include <wx/event.h>

class c_ImageAlignmentWorkerThread;

namespace scripting
{

/// Handles script-made image processing requests; runs in the main thread.
class ScriptImageProcessor
{
public:
    ScriptImageProcessor(
        std::unique_ptr<imppg::backend::IProcessingBackEnd>&& processor,
        bool normalizeFitsValues
    );

    ~ScriptImageProcessor();

    void StartProcessing(
        MessageContents request,
        std::shared_ptr<Heartbeat> heartbeat,
        std::function<void(FunctionCallResult)> onCompletion ///< Receives error message on error.
    );

    void AbortProcessing();

    void OnIdle(wxIdleEvent& event);

private:
    using CompletionFunc = std::function<void(FunctionCallResult)>;

    void OnProcessImageFile(const contents::ProcessImageFile& call, CompletionFunc onCompletion);
    void OnProcessImage(const contents::ProcessImage& call, CompletionFunc onCompletion);
    void OnAlignRGB(const contents::AlignRGB& call, CompletionFunc onCompletion);
    void OnAlignImages(
        const contents::AlignImages& call,
        std::shared_ptr<Heartbeat> heartbeat,
        CompletionFunc onCompletion
    );

    std::unique_ptr<imppg::backend::IProcessingBackEnd> m_Processor;
    bool m_NormalizeFitsValues{false};
    std::unique_ptr<c_ImageAlignmentWorkerThread> m_AlignmentWorker;
    std::unique_ptr<wxEvtHandler> m_AlignmentEvtHandler;
};

}
