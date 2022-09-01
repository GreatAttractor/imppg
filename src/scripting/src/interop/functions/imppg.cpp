#include "interop/classes/DummyObject1.h"
#include "interop/classes/DummyObject2.h"
#include "interop/classes/ImageWrapper.h"
#include "interop/classes/SettingsWrapper.h"
#include "interop/functions/common.h"
#include "interop/functions/imppg.h"
#include "interop/state.h"

// private definitions
namespace
{

void Dummy()
{
    scripting::g_State->CallFunctionAndAwaitCompletion(scripting::call::Dummy{});
}

} // end of private definitions

namespace scripting::functions
{

const luaL_Reg imppg[] = {
    {"handler1", [](lua_State*) -> int { Dummy(); return 0; }},

    {"create_dummy1", [](lua_State* lua) -> int {
        /*DummyObject1* object =*/ new(PrepareObject<DummyObject1>(lua)) DummyObject1();
        return 1;
    }},

    {"create_dummy2", [](lua_State* lua) -> int {
        int i = luaL_checkinteger(lua, 1);
        new(PrepareObject<DummyObject2>(lua)) DummyObject2(i);
        return 1;
    }},

    {"new_settings", [](lua_State* lua) -> int {
        new(PrepareObject<SettingsWrapper>(lua)) SettingsWrapper();
        return 1;
    }},

    {"load_image_as_mono32f", [](lua_State* lua) -> int {
        const std::string imagePath = GetString(lua, 1);
        new(PrepareObject<ImageWrapper>(lua)) ImageWrapper(imagePath);
        return 1;
    }},

    //TODO: rename?
    {"process_image", [](lua_State* lua) -> int {
        const std::string imagePath = GetString(lua, 1);
        const std::string settingsPath = GetString(lua, 2);
        const std::string outputImagePath = GetString(lua, 3);

        scripting::g_State->CallFunctionAndAwaitCompletion(scripting::call::ProcessImage{imagePath, settingsPath,
            outputImagePath});

        return 0; //TODO: return processing result
    }},

    {nullptr, nullptr} // end-of-data marker
};

}
