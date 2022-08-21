#pragma once

#include "common/proc_settings.h"

#include <lua.hpp>

namespace scripting
{

class SettingsWrapper
{
public:
    static const luaL_Reg* GetMethods()
    {
        static const luaL_Reg methods[] = {

            {nullptr, nullptr} // end-of-data marker
        };

        return methods;
    }

    const ProcessingSettings& GetSettings() const;

private:
    ProcessingSettings m_SettingsWrapper;
};

}
