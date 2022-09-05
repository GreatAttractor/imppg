#include "interop/classes/DirectoryIterator.h"
#include "interop/functions/common.h"
#include "interop/functions/imppg_filesystem.h"
#include "interop/state.h"

#include <filesystem>

namespace scripting::functions
{

const luaL_Reg imppg_filesystem[] = {
    {"list_files", [](lua_State* lua) -> int {
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

}
