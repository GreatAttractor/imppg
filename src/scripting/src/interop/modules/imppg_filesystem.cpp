/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2023-2025 Filip Szczerek <ga.software@yahoo.com>

This file is part of ImPPG.

ImPPG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

ImPPG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ImPPG.  If not, see <http://www.gnu.org/licenses/>.

File description:
    File system module implementation.
*/

#include "interop/classes/DirectoryIterator.h"
#include "interop/modules/common.h"
#include "interop/modules/imppg_filesystem.h"
#include "interop/state.h"

#include <algorithm>
#include <filesystem>

namespace scripting::modules::imppg::filesystem
{

const luaL_Reg functions[] = {
    {"list_files", [](lua_State* lua) -> int {
        CheckNumArgs(lua, "list_files", 1);
        const auto fileNamePattern = scripting::GetString(lua, 1);
        auto object = DirectoryIterator::Create(std::move(fileNamePattern));
        new(PrepareObject<DirectoryIterator>(lua)) DirectoryIterator(std::move(object));
        lua_pushcclosure(lua, [](lua_State* lua) {
            auto* object = static_cast<DirectoryIterator*>(
                luaL_checkudata(lua, lua_upvalueindex(1), typeid(DirectoryIterator).name())
            );
            if (const auto result = object->Next())
            {
                lua_pushstring(lua, result->GetFullPath().utf8_str());
                return 1;
            }
            else
            {
                return 0;
            }
        }, 1);

        return 1;
    }},

    {"list_files_sorted", [](lua_State* lua) -> int {
        CheckNumArgs(lua, "list_files_sorted", 1);
        const auto fileNamePattern = scripting::GetString(lua, 1);

        DirectoryIterator dirIter = DirectoryIterator::Create(std::move(fileNamePattern));

        std::vector<wxString> files;
        while (const auto file = dirIter.Next())
        {
            files.emplace_back(file.value().GetFullPath());
        }
        std::sort(files.begin(), files.end());

        lua_newtable(lua);
        std::size_t index{0};
        for (const auto& file: files)
        {
            lua_pushinteger(lua, index + 1);
            lua_pushstring(lua, file.utf8_str());
            lua_settable(lua, 2); // returned table is at 2 (1 contains the argument (file name pattern))
            ++index;
        }

        lua_pushinteger(lua, index);

        return 2;
    }},

    {nullptr, nullptr} // end-of-data marker
};

const std::vector<std::pair<std::string, int>> constants{};

}
