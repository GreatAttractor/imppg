#pragma once

#include "scripting/interop.h"

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
    State(wxEvtHandler& parent): m_Parent(parent) {}

    /// If the result is `Error`, throws automatically.
    FunctionCallResult CallFunctionAndAwaitCompletion(FunctionCall functionCall);

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

private:
    void OnObjectCreatedImpl(const char* typeName);

    void OnObjectDestroyedImpl(const char* typeName);

    wxEvtHandler& m_Parent;

    // key: typeid name
    std::unordered_map<std::string, std::size_t> m_ObjectCounts;
};

extern std::unique_ptr<State> g_State;

}
