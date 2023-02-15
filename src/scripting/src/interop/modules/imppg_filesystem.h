#pragma once

#include <lua.hpp>
#include <string>
#include <utility>
#include <vector>

namespace scripting::modules::imppg::filesystem
{
extern const luaL_Reg functions[];
extern const std::vector<std::pair<std::string, int>> constants;
}
