#pragma once

#include "scripting/interop.h"

#include <lua.hpp>
#include <future>
#include <memory>
#include <string>
#include <typeinfo>
#include <unordered_map>

class wxEvtHandler;

namespace scripting
{

class State
{
public:
    State(wxEvtHandler& parent, std::future<void>&& stopRequested)
    : m_Parent(parent)
    , m_StopRequested(std::move(stopRequested))
    {}

    void SendMessage(MessageContents&& message);

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

    wxEvtHandler& m_Parent;

    std::future<void> m_StopRequested;

    // key: typeid name
    std::unordered_map<std::string, std::size_t> m_ObjectCounts;
};

extern std::unique_ptr<State> g_State;

}
