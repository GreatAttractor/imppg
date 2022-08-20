#pragma once

#include "interop/state.h"

#include <lua.hpp>
#include <string>

namespace scripting
{

/// Returns pointer to be used for placement-new constructor call.
template<typename T>
T* PrepareObject(lua_State* lua)
{
    auto** pptr = static_cast<T**>(lua_newuserdata(lua, sizeof(T*)));
    *pptr = static_cast<T*>((operator new[])(sizeof(T)));
    luaL_newmetatable(lua, typeid(T).name());
    lua_setmetatable(lua, -2);
    g_State->OnObjectCreated<T>();
    return *pptr;
}

std::string GetString(lua_State* lua, int stackPos);

}
