#include "interop/functions/common.h"

namespace scripting
{

std::string GetString(lua_State* lua, int stackPos)
{
    luaL_checktype(lua, stackPos, LUA_TSTRING);
    std::size_t length{0};
    const char* contents = lua_tolstring(lua, stackPos, &length);
    return std::string{contents, length};
}

}
