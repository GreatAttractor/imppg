#pragma once

#include "interop/classes/method.h"

#include <lua.hpp>

namespace scripting
{

class DummyObject1
{
public:
    int get() const;

    static const luaL_Reg* GetMethods()
    {
        static const luaL_Reg methods[] = {
            {"get", [](lua_State* lua) { return ConstMethodIntResult<DummyObject1>(lua, &DummyObject1::get); }},

            {nullptr, nullptr} // end-of-data marker
        };

        return methods;
    }
};

}
