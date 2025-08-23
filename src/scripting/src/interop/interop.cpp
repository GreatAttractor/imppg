/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2022-2025 Filip Szczerek <ga.software@yahoo.com>

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
    Script interoperability implementation.
*/

#include "interop/classes/DirectoryIterator.h"
#include "interop/classes/ImageWrapper.h"
#include "interop/classes/SettingsWrapper.h"
#include "interop/modules/imppg_filesystem.h"
#include "interop/modules/imppg_test.h"
#include "interop/modules/imppg.h"
#include "interop/state.h"
#include "scripting/interop.h"

#include <functional>
#include <lua.hpp>
#include <memory>

namespace scripting
{

// private definitions
namespace
{

template<typename T>
int GarbageCollectHandler(lua_State* lua)
{
    void* storage = luaL_checkudata(lua, 1, typeid(T).name());
    auto* object = static_cast<T*>(storage);
    object->~T();
    g_State->OnObjectDestroyed<T>();
    return 0;
}

template<typename T>
const luaL_Reg* GetMetatable()
{
    static const luaL_Reg metatable[] = {
        {"__gc", [](lua_State* lua) { return GarbageCollectHandler<T>(lua); }},

        {nullptr, nullptr} // end-of-data marker
    };

    return metatable;
}

template<typename T>
void RegisterClass(lua_State* lua)
{
    luaL_newmetatable(lua, typeid(T).name());
    luaL_setfuncs(lua, GetMetatable<T>(), 0);
    lua_newtable(lua);
    luaL_setfuncs(lua, T::GetMethods(), 0);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);
}

template<typename T>
void RegisterIterator(lua_State* lua)
{
    luaL_newmetatable(lua, typeid(T).name());
    luaL_setfuncs(lua, GetMetatable<T>(), 0);
    lua_pop(lua, 1);
}

class ModuleRegistratorBase
{
public:
    ModuleRegistratorBase(
        lua_State* lua,
        const char* moduleName,
        const luaL_Reg* functions,
        const std::vector<std::pair<std::string, int>>& constants
    ): m_ModuleName(moduleName), m_Lua(lua)
    {
        lua_newtable(m_Lua);
        luaL_setfuncs(m_Lua, functions, 0);
        for (const auto& c: constants)
        {
            lua_pushinteger(m_Lua, c.second);
            lua_setfield(m_Lua, -2, c.first.c_str());
        }
    }

protected:
    const char* m_ModuleName{nullptr};
    lua_State* m_Lua{nullptr};
};

class ModuleRegistrator final: public ModuleRegistratorBase
{
public:
    ModuleRegistrator(
        lua_State* lua,
        const char* moduleName,
        const luaL_Reg* functions,
        const std::vector<std::pair<std::string, int>>& constants
    ): ModuleRegistratorBase(lua, moduleName, functions, constants)
    {}

    ~ModuleRegistrator()
    {
        lua_setglobal(m_Lua, m_ModuleName);
    }
};

class SubmoduleRegistrator final: public ModuleRegistratorBase
{
public:
    SubmoduleRegistrator(
        lua_State* lua,
        const char* moduleName,
        const luaL_Reg* functions,
        const std::vector<std::pair<std::string, int>>& constants
    ): ModuleRegistratorBase(lua, moduleName, functions, constants)
    {}

    ~SubmoduleRegistrator()
    {
        lua_setfield(m_Lua, -2, m_ModuleName);
    }
};

#define BEGIN_MODULE(name, namespace) \
    { const ModuleRegistrator modreg__LINE__(lua, name, namespace::functions, namespace::constants);

#define END_MODULE() }

#define BEGIN_SUBMODULE(name, namespace) \
    { const SubmoduleRegistrator submodreg__LINE__(lua, name, namespace::functions, namespace::constants);

#define END_SUBMODULE() }

} // end of private definitions

void Prepare(lua_State* lua, wxEvtHandler& parent, std::future<void>&& stopRequested)
{
    g_State = std::make_unique<State>(lua, parent, std::move(stopRequested));

    BEGIN_MODULE("imppg", scripting::modules::imppg);
        BEGIN_SUBMODULE("filesystem", scripting::modules::imppg::filesystem);
        END_SUBMODULE();

        BEGIN_SUBMODULE("test", scripting::modules::imppg::test);
            BEGIN_SUBMODULE("test", scripting::modules::imppg::test);
            END_SUBMODULE();
        END_SUBMODULE();
    END_MODULE();

    RegisterClass<ImageWrapper>(lua);
    RegisterClass<SettingsWrapper>(lua);

    RegisterIterator<DirectoryIterator>(lua);
}

void Finish()
{
    g_State->VerifyAllObjectsRemoved();
    g_State.reset();
}

}
