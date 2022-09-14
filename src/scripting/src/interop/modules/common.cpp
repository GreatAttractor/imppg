#include "interop/modules/common.h"
#include "scripting/script_exceptions.h"

namespace scripting
{

std::string GetString(lua_State* lua, int stackPos)
{
    luaL_checktype(lua, stackPos, LUA_TSTRING);
    std::size_t length{0};
    const char* contents = lua_tolstring(lua, stackPos, &length);
    return std::string{contents, length};
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

}
