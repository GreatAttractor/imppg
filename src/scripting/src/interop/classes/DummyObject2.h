#pragma once

#include "interop/classes/method.h"

#include <lua.hpp>

namespace scripting
{

class DummyObject2
{
public:
    DummyObject2(int i);

    int get(int n) const;

    static const luaL_Reg* GetMethods()
    {
        static const luaL_Reg methods[] = {
            {"get", [](lua_State* lua) { return ConstMethodIntArgIntResult<DummyObject2>(lua, &DummyObject2::get); }},

            {nullptr, nullptr} // end-of-data marker
        };

        return methods;
    }

private:
    int m_I;
};

}
