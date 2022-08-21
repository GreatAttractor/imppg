#include "interop/classes/ImageWrapper.h"
#include "interop/classes/SettingsWrapper.h"
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

void NotifySettings(const scripting::SettingsWrapper& s)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::call::NotifySettings{s.GetSettings()});
}

void NotifyImage(const scripting::ImageWrapper& img)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::call::NotifyImage{img.GetImage()});
}

} // end of private definitions

namespace scripting::functions
{

const luaL_Reg imppg_test[] = {
    {"notify_string", [](lua_State* lua) -> int { NotifyString(scripting::GetString(lua, 1)); return 0; }},
    {"notify_settings", [](lua_State* lua) -> int { NotifySettings(scripting::GetObject<SettingsWrapper>(lua, 1)); return 0; }},
    {"notify_image", [](lua_State* lua) -> int { NotifyImage(scripting::GetObject<ImageWrapper>(lua, 1)); return 0; }},

    {nullptr, nullptr} // end-of-data marker
};

}
