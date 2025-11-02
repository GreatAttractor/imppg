/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2023-2025 Filip Szczerek <ga.software@yahoo.com>

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
    Test utility implementation.
*/

#include "interop/classes/ImageWrapper.h"
#include "interop/classes/SettingsWrapper.h"
#include "interop/modules/common.h"
#include "interop/modules/imppg_test.h"
#include "interop/state.h"

// private definitions
namespace
{

void NotifyString(wxString s, bool ordered)
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
