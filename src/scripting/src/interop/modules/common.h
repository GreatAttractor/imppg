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
    Common module utility declarations.
*/

#pragma once

#include "interop/state.h"

#include <lua.hpp>
#include <string>
#include <vector>

namespace scripting
{

/// Returns pointer to be used for placement-new constructor call.
/// NOTE: Must not be called if the subsequently called constructor may throw. In that case,
/// the failed-construction object would destroy its members, and then they would be destroyed again
/// via `scripting::GarbageCollectHandler` (leading to possible memory corruption.)
///
/// Example:
///
/// GOOD:
/// ```c++
/// auto object = MyClass::Create(param1, param2); // may throw
/// new(PrepareObject<MyClass>(lua)) MyClass(std::move(object)); // move ctor does not throw
/// ```
///
/// BAD:
/// ```c++
/// new(PrepareObject<MyClass>(lua)) MyClass(param1, param2); // may throw
/// ```
///
template<typename T>
void* PrepareObject(lua_State* lua)
{
    void* storage = lua_newuserdata(lua, sizeof(T));
    luaL_newmetatable(lua, typeid(T).name());
    lua_setmetatable(lua, -2);
    g_State->OnObjectCreated<T>();
    return storage;
}

template<typename T>
T& GetObject(lua_State* lua, int stackIndex)
{
    return *static_cast<T*>(luaL_checkudata(lua, stackIndex, typeid(T).name()));
}

wxString GetString(lua_State* lua, int stackPos);

int GetInteger(lua_State* lua, int stackPos);

double GetNumber(lua_State* lua, int stackPos);

bool GetBoolean(lua_State* lua, int stackPos);

std::vector<wxString> GetStringTable(lua_State* lua, int stackPos);

void CheckNumArgs(lua_State* lua, const char* functionName, int expectedNum);

void CheckType(lua_State* lua, int stackPos, int type, bool allowNil = false);

}
