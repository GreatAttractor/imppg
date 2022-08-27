#pragma once

#include "interop/functions/common.h"

#include <lua.hpp>
#include <typeinfo>

namespace scripting
{

template<typename T>
int MethodNoResult(lua_State* lua, void (T::* method)())
{
    auto* object = *static_cast<T**>(luaL_checkudata(lua, 1, typeid(T).name()));
    (object.*method)();
    return 0;
}

template<typename T>
int ConstMethodIntResult(lua_State* lua, int (T::* method)() const)
{
    auto* object = *static_cast<T**>(luaL_checkudata(lua, 1, typeid(T).name()));
    lua_pushinteger(lua, (object->*method)());
    return 1;
}

template<typename T>
int ConstMethodDoubleResult(lua_State* lua, double (T::* method)() const)
{
    auto* object = *static_cast<T**>(luaL_checkudata(lua, 1, typeid(T).name()));
    lua_pushnumber(lua, (object->*method)());
    return 1;
}

template<typename T>
int ConstMethodBoolResult(lua_State* lua, bool (T::* method)() const)
{
    auto* object = *static_cast<T**>(luaL_checkudata(lua, 1, typeid(T).name()));
    lua_pushboolean(lua, (object->*method)());
    return 1;
}

template<typename T>
int ConstMethodIntArgIntResult(lua_State* lua, int (T::* method)(int) const)
{
    auto* object = *static_cast<T**>(luaL_checkudata(lua, 1, typeid(T).name()));
    int i = luaL_checkinteger(lua, 2);
    lua_pushinteger(lua, (object->*method)(i));
    return 1;
}

template<typename T>
int MethodIntArg(lua_State* lua, void (T::* method)(int))
{
    auto* object = *static_cast<T**>(luaL_checkudata(lua, 1, typeid(T).name()));
    int i = luaL_checkinteger(lua, 2);
    (object->*method)(i);
    return 0;
}

template<typename T>
int MethodDoubleArg(lua_State* lua, void (T::* method)(double))
{
    auto* object = *static_cast<T**>(luaL_checkudata(lua, 1, typeid(T).name()));
    (object->*method)(GetNumber(lua, 2));
    return 0;
}

template<typename T>
int MethodBoolArg(lua_State* lua, void (T::* method)(bool))
{
    auto* object = *static_cast<T**>(luaL_checkudata(lua, 1, typeid(T).name()));
    (object->*method)(GetBoolean(lua, 2));
    return 0;
}

}
