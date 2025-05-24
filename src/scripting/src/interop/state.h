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
    Script state declarations.
*/

#pragma once

#include "scripting/interop.h"

#include <lua.hpp>
#include <future>
#include <memory>
#include <shared_mutex>
#include <string>
#include <typeinfo>
#include <unordered_map>

class wxEvtHandler;

namespace scripting
{

class State;

class MessageSender
{
public:
    void SendMessage(MessageContents&& message);

private:
    MessageSender(wxEvtHandler& handler): m_EvtHandler(handler) {}

    void Invalidate();

    wxEvtHandler& m_EvtHandler;
    struct
    {
        std::shared_mutex guard;
        bool valid{true};
    } m_Valid{};

    friend class State;
};

// TODO except ctor, remove `lua` params
class State
{
public:
    State(lua_State* lua, wxEvtHandler& parent, std::future<void>&& stopRequested)
    : m_Lua(lua)
    , m_Parent(parent)
    , m_StopRequested(std::move(stopRequested))
    , m_MsgSender{std::shared_ptr<MessageSender>(new MessageSender(m_Parent))}
    {}

    ~State() { m_MsgSender->Invalidate(); }

    std::weak_ptr<MessageSender> Sender() const { return m_MsgSender; }

    /// If the result is `Error`, throws automatically.
    FunctionCallResult CallFunctionAndAwaitCompletion(MessageContents&& functionCall);

    template<typename T>
    void OnObjectCreated()
    {
        OnObjectCreatedImpl(typeid(T).name());
    }

    template<typename T>
    void OnObjectDestroyed()
    {
        OnObjectDestroyedImpl(typeid(T).name());
    }

    void VerifyAllObjectsRemoved() const;

    bool CheckStopRequested(lua_State* lua);

private:
    void OnObjectCreatedImpl(const char* typeName);

    void OnObjectDestroyedImpl(const char* typeName);

    lua_State* m_Lua{nullptr};

    wxEvtHandler& m_Parent;

    std::future<void> m_StopRequested;

    // key: typeid name
    std::unordered_map<std::string, std::size_t> m_ObjectCounts;

    std::shared_ptr<MessageSender> m_MsgSender;
};

extern std::unique_ptr<State> g_State;

}
