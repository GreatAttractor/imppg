#include <future>
#include <lua.hpp>
#include <wx/event.h>

namespace scripting
{

/// Prepares interop for script execution.
void Prepare(lua_State* lua, wxEvtHandler& parent, std::future<void>&& stopRequested);

/// Cleans up interop state.
void Finish();

}
