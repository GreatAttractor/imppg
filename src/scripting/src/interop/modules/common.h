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

std::string GetString(lua_State* lua, int stackPos);

int GetInteger(lua_State* lua, int stackPos);

double GetNumber(lua_State* lua, int stackPos);

bool GetBoolean(lua_State* lua, int stackPos);

std::vector<std::string> GetStringTable(lua_State* lua, int stackPos);

void CheckNumArgs(lua_State* lua, const char* functionName, int expectedNum);

void CheckType(lua_State* lua, int stackPos, int type, bool allowNil = false);

}
