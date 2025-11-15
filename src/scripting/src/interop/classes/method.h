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
    Method utilities.
*/

#pragma once

#include "interop/modules/common.h"

#include <lua.hpp>
#include <typeinfo>

namespace scripting
{

template<typename T>
const T* GetConstObject(lua_State* lua)
{
    return static_cast<const T*>(luaL_checkudata(lua, 1, typeid(T).name()));
}

template<typename T>
T* GetObject(lua_State* lua)
{
    return static_cast<T*>(luaL_checkudata(lua, 1, typeid(T).name()));
}

template<typename T>
int MethodNoResult(lua_State* lua, void (T::* method)())
{
    auto* object = GetObject<T>(lua);
    (object->*method)();
    return 0;
}

template<typename T>
int ConstMethodIntResult(lua_State* lua, int (T::* method)() const)
{
    const auto* object = GetConstObject<T>(lua);
    lua_pushinteger(lua, (object->*method)());
    return 1;
}

template<typename T>
int ConstMethodDoubleResult(lua_State* lua, double (T::* method)() const)
{
    const auto* object = GetConstObject<T>(lua);
    lua_pushnumber(lua, (object->*method)());
    return 1;
}

template<typename T>
int ConstMethodBoolResult(lua_State* lua, bool (T::* method)() const)
{
    const auto* object = GetConstObject<T>(lua);
    lua_pushboolean(lua, (object->*method)());
    return 1;
}

template<typename T>
int ConstMethodIntArgIntResult(lua_State* lua, int (T::* method)(int) const)
{
    const auto* object = GetConstObject<T>(lua);
    int i = luaL_checkinteger(lua, 2);
    lua_pushinteger(lua, (object->*method)(i));
    return 1;
}

template<typename T>
int ConstMethodIntArgDoubleResult(lua_State* lua, double (T::* method)(int) const)
{
    const auto* object = GetConstObject<T>(lua);
    int i = luaL_checkinteger(lua, 2);
    lua_pushnumber(lua, (object->*method)(i));
    return 1;
}

template<typename T>
int ConstMethodIntArgBoolResult(lua_State* lua, bool (T::* method)(int) const)
{
    const auto* object = GetConstObject<T>(lua);
    int i = luaL_checkinteger(lua, 2);
    lua_pushboolean(lua, (object->*method)(i));
    return 1;
}

template<typename T>
int MethodIntArg(lua_State* lua, void (T::* method)(int))
{
    auto* object = GetObject<T>(lua);
    int i = luaL_checkinteger(lua, 2);
    (object->*method)(i);
    return 0;
}

template<typename T>
int MethodDoubleArg(lua_State* lua, void (T::* method)(double))
{
    auto* object = GetObject<T>(lua);
    (object->*method)(GetNumber(lua, 2));
    return 0;
}

template<typename T>
int MethodBoolArg(lua_State* lua, void (T::* method)(bool))
{
    auto* object = GetObject<T>(lua);
    (object->*method)(GetBoolean(lua, 2));
    return 0;
}

template<typename T>
int MethodIntBoolArg(lua_State* lua, void (T::* method)(int, bool))
{
    auto* object = GetObject<T>(lua);
    (object->*method)(GetInteger(lua, 2), GetBoolean(lua, 3));
    return 0;
}

template<typename T>
int MethodDoubleDoubleArg(lua_State* lua, void (T::* method)(double, double))
{
    auto* object = GetObject<T>(lua);
    (object->*method)(GetNumber(lua, 2), GetNumber(lua, 3));
    return 0;
}

template<typename T>
int MethodIntDoubleArg(lua_State* lua, void (T::* method)(int, double))
{
    auto* object = GetObject<T>(lua);
    (object->*method)(GetInteger(lua, 2), GetNumber(lua, 3));
    return 0;
}

template<typename T>
int MethodDoubleDoubleArgIntResult(lua_State* lua, int (T::* method)(double, double))
{
    auto* object = GetObject<T>(lua);
    lua_pushinteger(lua, (object->*method)(GetNumber(lua, 2), GetNumber(lua, 3)));
    return 1;
}

template<typename T>
int MethodIntDoubleDoubleArg(lua_State* lua, void (T::* method)(int, double, double))
{
    auto* object = GetObject<T>(lua);
    (object->*method)(GetInteger(lua, 2), GetNumber(lua, 3), GetNumber(lua, 4));
    return 0;
}

template<typename T>
int ConstMethodStringIntArg(lua_State* lua, void (T::* method)(const wxString&, int) const)
{
    const auto* object = GetConstObject<T>(lua);
    (object->*method)(GetString(lua, 2), GetInteger(lua, 3));
    return 0;
}

}
