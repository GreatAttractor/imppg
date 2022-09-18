#include "scripting/interop.h"
#include "interop/state.h"

#include <boost/lexical_cast.hpp>
#include <wx/event.h>
#include <wx/intl.h>

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

void State::SendMessage(MessageContents&& message)
{
    auto* event = new wxThreadEvent(wxEVT_THREAD);
    event->SetPayload(ScriptMessagePayload{std::move(message)});
    m_Parent.QueueEvent(event);
}

FunctionCallResult State::CallFunctionAndAwaitCompletion(MessageContents&& functionCall)
{
    auto* event = new wxThreadEvent(wxEVT_THREAD);
    std::promise<FunctionCallResult> completionSend;
    std::future<FunctionCallResult> completionRecv = completionSend.get_future();
    event->SetPayload(ScriptMessagePayload{std::move(functionCall), std::move(completionSend)});
    m_Parent.QueueEvent(event);
    const FunctionCallResult result = completionRecv.get();
    if (auto* error = std::get_if<call_result::Error>(&result))
    {
        throw ScriptExecutionError(error->message);
    }

    return result;
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
