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

std::vector<std::string> GetStringTable(lua_State* lua, int stackPos)
{
    luaL_checktype(lua, stackPos, LUA_TTABLE);
    const std::size_t len = lua_rawlen(lua, stackPos);
    std::vector<std::string> result;
    result.reserve(len);
    for (std::size_t i = 1; i <= len; ++i)
    {
        const int valueType = lua_rawgeti(lua, stackPos, i);
        if (valueType != LUA_TSTRING) { throw ScriptExecutionError{"expected: string"}; }
        result.push_back(GetString(lua, lua_gettop(lua)));
    }
    lua_pop(lua, len);
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
