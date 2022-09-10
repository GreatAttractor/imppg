/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2022 Filip Szczerek <ga.software@yahoo.com>

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

#include <memory>
#include <wx/event.h>

namespace scripting
{

/// Handles script-made image processing requests; runs in the main thread.
class ScriptImageProcessor
{
public:
    ScriptImageProcessor(std::unique_ptr<imppg::backend::IProcessingBackEnd> processor);

    void HandleProcessingRequest(ScriptMessagePayload&& request);

    void OnIdle(wxIdleEvent& event);

private:
    std::unique_ptr<imppg::backend::IProcessingBackEnd> m_Processor;
};

}
