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
    Script image processor implementation.
*/

#include "../../imppg_assert.h"
#include "scripting/script_image_processor.h"

// private definitions
namespace
{

template<typename ... Ts>
struct Overload : Ts ... {
    using Ts::operator() ...;
};
template<class... Ts> Overload(Ts...) -> Overload<Ts...>;

}

namespace scripting
{

ScriptImageProcessor::ScriptImageProcessor(std::unique_ptr<imppg::backend::IProcessingBackEnd> processor)
: m_Processor(std::move(processor))
{
}

FunctionCallResult ScriptImageProcessor::HandleProcessingRequest(const FunctionCall& request)
{
    FunctionCallResult result = call_result::Success{};

    const auto handler = Overload{
        [&](const call::ProcessImageFile& call) {
            result = call_result::Error{"not implemented"};
        },

        [&](const call::ProcessImage& call) {
            result = call_result::Error{"not implemented"};
        },

        [](auto) { IMPPG_ABORT_MSG("invalid message passed to ScriptImageProcessor"); },
    };

    std::visit(handler, request);

    return result;
}

void ScriptImageProcessor::OnIdle(wxIdleEvent& event)
{
    m_Processor->OnIdle(event);
}

}
