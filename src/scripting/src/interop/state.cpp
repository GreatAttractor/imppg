#include "scripting/interop.h"
#include "interop/state.h"

#include <boost/lexical_cast.hpp>
#include <wx/event.h>

namespace scripting
{

std::unique_ptr<State> g_State;

void State::CallFunctionAndAwaitCompletion(FunctionCall functionCall)
{
    auto* event = new wxThreadEvent(wxEVT_THREAD, MessageId::ScriptFunctionCall);
    std::promise<FunctionCallResult> completionSend;
    std::future<FunctionCallResult> completionRecv = completionSend.get_future();
    event->SetPayload(ScriptMessagePayload(functionCall, std::move(completionSend)));
    m_Parent.QueueEvent(event);
    const FunctionCallResult result = completionRecv.get();
    if (auto* error = std::get_if<call_result::Error>(&result))
    {
        throw ScriptExecutionError(error->message);
    }
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
