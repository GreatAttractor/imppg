#include "interop/classes/ImageWrapper.h"
#include "interop/classes/SettingsWrapper.h"
#include "interop/modules/common.h"
#include "interop/modules/imppg_test.h"
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

void NotifyNumber(double number)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::call::NotifyNumber{number});
}

void NotifyBoolean(bool value)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::call::NotifyBoolean{value});
}

void NotifyInteger(int value)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::call::NotifyInteger{value});
}

} // end of private definitions

namespace scripting::modules::imppg::test
{

const luaL_Reg functions[] = {
    {"notify_boolean", [](lua_State* lua) -> int { NotifyBoolean(scripting::GetBoolean(lua, 1)); return 0; }},
    {"notify_integer", [](lua_State* lua) -> int { NotifyInteger(scripting::GetInteger(lua, 1)); return 0; }},
    {"notify_number", [](lua_State* lua) -> int { NotifyNumber(scripting::GetNumber(lua, 1)); return 0; }},
    {"notify_string", [](lua_State* lua) -> int { NotifyString(scripting::GetString(lua, 1)); return 0; }},
    {"notify_settings", [](lua_State* lua) -> int { NotifySettings(scripting::GetObject<SettingsWrapper>(lua, 1)); return 0; }},
    {"notify_image", [](lua_State* lua) -> int { NotifyImage(scripting::GetObject<ImageWrapper>(lua, 1)); return 0; }},

    {nullptr, nullptr} // end-of-data marker
};

const std::vector<std::pair<std::string, int>> constants{};

}
