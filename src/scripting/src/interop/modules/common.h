#pragma once

#include "interop/state.h"

#include <lua.hpp>
#include <string>

namespace scripting
{

/// Returns pointer to be used for placement-new constructor call.
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

}
