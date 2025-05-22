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
    Script runner thread implementation.
*/

#include "interop/interop_impl.h"
#include "interop/state.h"
#include "scripting/script_exceptions.h"
#include "scripting/interop.h"
#include "scripting/script_runner.h"

#include <array>
#include <lua.hpp>
#include <thread>
#include <sstream>

/// Expects that Lua state variable `lua` is in scope.
#define CHECKED_CALL(funcCall)                                                         \
    do                                                                                 \
    {                                                                                  \
        const auto result = (funcCall);                                                \
        if (LUA_OK != result)                                                          \
        {                                                                              \
            std::stringstream s;                                                       \
            s << "failure at " << __FILE__ << ":" << __LINE__                          \
              << ", error code: " << result << ", message: " << lua_tostring(lua, -1); \
            throw ScriptExecutionError(s.str());                                       \
        }                                                                              \
    } while (false);

// private definitions
namespace
{

struct StreamReaderState
{
    StreamReaderState(std::unique_ptr<std::istream> stream): stream(std::move(stream)) {}

    static constexpr std::size_t CHUNK_SIZE = 64 * 1024;
    std::array<char, CHUNK_SIZE> dataChunk;
    std::unique_ptr<std::istream> stream;
};

const char* StreamReader(lua_State*, void* opaqueReaderState, std::size_t* size)
{
    auto* state = static_cast<StreamReaderState*>(opaqueReaderState);
    state->stream->read(state->dataChunk.data(), StreamReaderState::CHUNK_SIZE);
    *size = state->stream->gcount();
    return state->dataChunk.data();
}

}

namespace scripting
{

ScriptRunner::ScriptRunner(
    std::unique_ptr<std::istream> script,
    wxEvtHandler& parent,
    std::future<void>&& stopRequested
)
: wxThread(wxTHREAD_JOINABLE)
, m_Script(std::move(script))
, m_Parent(parent)
, m_StopRequested(std::move(stopRequested))
{
}

wxThread::ExitCode ScriptRunner::Entry()
{
    lua_State *lua = luaL_newstate();

    try
    {
        if (!lua)
        {
            throw ScriptExecutionError{"failed to initialize Lua"};
        }
        luaL_openlibs(lua);

        lua_atpanic(lua, [](lua_State* lua) -> int {
            const char* error_msg = luaL_optstring(lua, 1, "unspecified error");
            throw ScriptExecutionError(error_msg);
            return 0;
        });

        scripting::Prepare(lua, m_Parent, std::move(m_StopRequested));

        auto readerState = StreamReaderState{std::move(m_Script)};
        CHECKED_CALL(lua_load(lua, &StreamReader, &readerState, "script", nullptr));
        lua_call(lua, 0, 0);
    }
    catch (const std::exception& exc)
    {
        auto* event = new wxThreadEvent(wxEVT_THREAD);
        event->SetPayload(ScriptMessagePayload{contents::Error{exc.what()}});
        m_Parent.QueueEvent(event);
    }

    lua_close(lua);
    scripting::Finish();

    auto* event = new wxThreadEvent(wxEVT_THREAD);
    event->SetPayload(ScriptMessagePayload{contents::ScriptFinished{}});
    m_Parent.QueueEvent(event);

    return static_cast<wxThread::ExitCode>(0);
}

ScriptRunner::~ScriptRunner()
{
    Wait();
}

}
