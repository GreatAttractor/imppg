#include "interop/classes/DirectoryIterator.h"
#include "interop/modules/common.h"
#include "interop/modules/imppg_filesystem.h"
#include "interop/state.h"

#include <filesystem>

namespace scripting::modules::imppg::filesystem
{

const luaL_Reg functions[] = {
    {"list_files", [](lua_State* lua) -> int {
        CheckNumArgs(lua, "list_files", 1);
        const auto fileNamePattern = scripting::GetString(lua, 1);
        new(PrepareObject<DirectoryIterator>(lua)) DirectoryIterator(std::move(fileNamePattern));
        lua_pushcclosure(lua, [](lua_State* lua) {
            auto* object = static_cast<DirectoryIterator*>(
                luaL_checkudata(lua, lua_upvalueindex(1), typeid(DirectoryIterator).name())
            );
            if (const auto result = object->Next())
            {
                lua_pushstring(lua, result->c_str());
                return 1;
            }
            else
            {
                return 0;
            }
        }, 1);

        return 1;
    }},

    {nullptr, nullptr} // end-of-data marker
};

const std::vector<std::pair<std::string, int>> constants{};

}
