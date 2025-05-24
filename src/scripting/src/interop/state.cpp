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
    Script state definitions.
*/

#include "scripting/interop.h"
#include "interop/state.h"

#include <boost/lexical_cast.hpp>
#include <wx/event.h>
#include <wx/intl.h>

using namespace std::chrono_literals;

namespace scripting
{

std::unique_ptr<State> g_State;

bool State::CheckStopRequested(lua_State* lua)
{
    if (m_StopRequested.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
    {
        lua_getglobal(lua, "error");
        lua_pushstring(lua, _("script interrupted by user").ToStdString().c_str());
        lua_call(lua, 1, 0);
        return true;
    }
    else
    {
        return false;
    }
}

void MessageSender::SendMessage(MessageContents&& message)
{
    std::shared_lock lock(m_Valid.guard);
    if (!m_Valid.valid) { return; }

    auto* event = new wxThreadEvent(wxEVT_THREAD);
    event->SetPayload(ScriptMessagePayload{std::move(message)});
    m_EvtHandler.QueueEvent(event);
}

void MessageSender::Invalidate()
{
    std::unique_lock lock(m_Valid.guard);
    m_Valid.valid = false;
}

FunctionCallResult State::CallFunctionAndAwaitCompletion(MessageContents&& functionCall)
{
    auto* event = new wxThreadEvent(wxEVT_THREAD);
    auto heartbeat = std::make_shared<Heartbeat>();
    event->SetPayload(ScriptMessagePayload{std::move(functionCall), heartbeat});
    m_Parent.QueueEvent(event);

    std::optional<FunctionCallResult> result;
    while (true)
    {
        if (CheckStopRequested(m_Lua))
        {
            // TODO Do sth more elegant (e.g., add and use `FunctionCallResult::Aborted`).
            throw ScriptExecutionError{"aborted by user"};
        }

        if (wxMSGQUEUE_TIMEOUT == heartbeat->ReceiveTimeout(/*10'000*/999'999, result))
        {
            throw ScriptExecutionError{"timed out waiting for command completion"};
        }

        if (result.has_value()) { break; }
    }

    if (auto* error = std::get_if<call_result::Error>(&result.value()))
    {
        throw ScriptExecutionError(error->message);
    }

    return result.value();
}

void State::OnObjectCreatedImpl(const char* typeName)
{
    m_ObjectCounts[typeName] += 1;
}

void State::OnObjectDestroyedImpl(const char* typeName)
{
    m_ObjectCounts[typeName] -= 1;
}

void State::VerifyAllObjectsRemoved() const
{
    for (const auto& [typeName, numObjects]: m_ObjectCounts)
    {
        if (numObjects != 0)
        {
            throw ScriptExecutionError(std::string{"expected 0 objects of type "} + typeName + ", but found "
                + boost::lexical_cast<std::string>(numObjects));
        }
    }
}

}
