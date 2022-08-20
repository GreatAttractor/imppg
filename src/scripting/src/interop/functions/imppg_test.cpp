#include "interop/functions/common.h"
#include "interop/functions/imppg_test.h"
#include "interop/state.h"

// private definitions
namespace
{

void NotifyString(std::string s)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::call::NotifyString{std::move(s)});
}

} // end of private definitions

namespace scripting::functions
{

const luaL_Reg imppg_test[] = {
    {"notify_string", [](lua_State* lua) -> int {
        NotifyString(scripting::GetString(lua, 1));
        return 0;
    }},

    {nullptr, nullptr} // end-of-data marker
};

}
