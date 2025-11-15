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
    Common module utility implementations.
*/

#include "interop/modules/common.h"
#include "scripting/script_exceptions.h"

namespace scripting
{

wxString GetString(lua_State* lua, int stackPos)
{
    luaL_checktype(lua, stackPos, LUA_TSTRING);
    std::size_t length{0};
    const char* contents = lua_tolstring(lua, stackPos, &length);
    wxString result = wxString::FromUTF8(contents, length);
    if (length != 0 && result.IsEmpty())
    {
        throw ScriptExecutionError{_("invalid UTF8 string")};
    }
    return result;
}

int GetInteger(lua_State* lua, int stackPos)
{
    int isNum{0};
    int value = lua_tointegerx(lua, stackPos, &isNum);
    if (!isNum) { throw ScriptExecutionError{"expected: integer value"}; }
    return value;
}

double GetNumber(lua_State* lua, int stackPos)
{
    luaL_checktype(lua, stackPos, LUA_TNUMBER);
    return lua_tonumber(lua, stackPos);
}

bool GetBoolean(lua_State* lua, int stackPos)
{
    luaL_checktype(lua, stackPos, LUA_TBOOLEAN);
    return lua_toboolean(lua, stackPos);
}

std::vector<wxString> GetStringTable(lua_State* lua, int stackPos)
{
    luaL_checktype(lua, stackPos, LUA_TTABLE);
    const std::size_t len = lua_rawlen(lua, stackPos);
    std::vector<wxString> result;
    result.reserve(len);
    for (std::size_t i = 1; i <= len; ++i)
    {
        const int valueType = lua_rawgeti(lua, stackPos, i);
        if (valueType != LUA_TSTRING) { throw ScriptExecutionError{"expected: string"}; }
        result.push_back(GetString(lua, lua_gettop(lua)));
        lua_pop(lua, 1);
    }
    return result;
}

void CheckNumArgs(lua_State* lua, const char* functionName, int expectedNum)
{
    const int numArgs = lua_gettop(lua);
    if (numArgs != expectedNum)
    {
        throw ScriptExecutionError{
            wxString::Format(_("expected %d arguments to %s"), expectedNum, functionName).ToStdString()
        };
    }
}

void CheckType(lua_State* lua, int stackPos, int type, bool allowNil)
{
    if (allowNil && lua_isnil(lua, stackPos))
    {
        return;
    }
    luaL_checktype(lua, stackPos, type);
}

}
