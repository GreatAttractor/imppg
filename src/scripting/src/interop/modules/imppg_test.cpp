#include "interop/classes/ImageWrapper.h"
#include "interop/classes/SettingsWrapper.h"
#include "interop/modules/common.h"
#include "interop/modules/imppg_test.h"
#include "interop/state.h"

// private definitions
namespace
{

void NotifyString(std::string s, bool ordered)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::contents::NotifyString{std::move(s), ordered});
}

void NotifySettings(const scripting::SettingsWrapper& s)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::contents::NotifySettings{s.GetSettings()});
}

void NotifyImage(const scripting::ImageWrapper& img)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::contents::NotifyImage{img.GetImage()});
}

void NotifyNumber(double number)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::contents::NotifyNumber{number});
}

void NotifyBoolean(bool value)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::contents::NotifyBoolean{value});
}

void NotifyInteger(int value)
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::contents::NotifyInteger{value});
}

} // end of private definitions

namespace scripting::modules::imppg::test
{

const luaL_Reg functions[] = {
    {"notify_boolean", [](lua_State* lua) -> int { NotifyBoolean(scripting::GetBoolean(lua, 1)); return 0; }},
    {"notify_integer", [](lua_State* lua) -> int { NotifyInteger(scripting::GetInteger(lua, 1)); return 0; }},
    {"notify_number", [](lua_State* lua) -> int { NotifyNumber(scripting::GetNumber(lua, 1)); return 0; }},
    {"notify_string", [](lua_State* lua) -> int { NotifyString(scripting::GetString(lua, 1), true); return 0; }},
    {"notify_string_unordered", [](lua_State* lua) -> int { NotifyString(scripting::GetString(lua, 1), false); return 0; }},
    {"notify_settings", [](lua_State* lua) -> int { NotifySettings(scripting::GetObject<SettingsWrapper>(lua, 1)); return 0; }},
    {"notify_image", [](lua_State* lua) -> int { NotifyImage(scripting::GetObject<ImageWrapper>(lua, 1)); return 0; }},

    {nullptr, nullptr} // end-of-data marker
};

const std::vector<std::pair<std::string, int>> constants{};

}
