/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2022 Filip Szczerek <ga.software@yahoo.com>

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
#include "interop/classes/DummyObject1.h"
#include "interop/classes/DummyObject2.h"
#include "interop/functions/imppg_filesystem.h"
#include "interop/functions/imppg_test.h"
#include "interop/functions/imppg.h"
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

class ModuleRegistrator
{
public:
    ModuleRegistrator(lua_State* lua, const char* moduleName, const luaL_Reg* functions)
    : m_ModuleName(moduleName), m_Lua(lua)
    {
        lua_newtable(m_Lua);
        luaL_setfuncs(m_Lua, functions, 0);
    }

    ~ModuleRegistrator()
    {
        lua_setglobal(m_Lua, m_ModuleName);
    }

private:
    const char* m_ModuleName{nullptr};
    lua_State* m_Lua{nullptr};
};

class SubmoduleRegistrator
{
public:
    SubmoduleRegistrator(lua_State* lua, const char* moduleName, const luaL_Reg* functions)
    : m_ModuleName(moduleName), m_Lua(lua)
    {
        lua_newtable(m_Lua);
        luaL_setfuncs(m_Lua, functions, 0);
    }

    ~SubmoduleRegistrator()
    {
        lua_setfield(m_Lua, -2, m_ModuleName);
    }

private:
    const char* m_ModuleName{nullptr};
    lua_State* m_Lua{nullptr};
};

#define BEGIN_MODULE(name, functions) { const ModuleRegistrator modreg__LINE__(lua, name, functions);

#define END_MODULE() }

#define BEGIN_SUBMODULE(name, functions) { const SubmoduleRegistrator submodreg__LINE__(lua, name, functions);

#define END_SUBMODULE() }

} // end of private definitions

void Prepare(lua_State* lua, wxEvtHandler& parent)
{
    g_State = std::make_unique<State>(parent);

    BEGIN_MODULE("imppg", scripting::functions::imppg);
        BEGIN_SUBMODULE("filesystem", scripting::functions::imppg_filesystem);
        END_SUBMODULE();

        BEGIN_SUBMODULE("test", scripting::functions::imppg_test);
            BEGIN_SUBMODULE("test", scripting::functions::imppg_test);
            END_SUBMODULE();
        END_SUBMODULE();
    END_MODULE();

    RegisterClass<DummyObject1>(lua);
    RegisterClass<DummyObject2>(lua);
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
