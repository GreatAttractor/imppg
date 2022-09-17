#include "common/formats.h"
#include "interop/classes/DummyObject1.h"
#include "interop/classes/DummyObject2.h"
#include "interop/classes/ImageWrapper.h"
#include "interop/classes/SettingsWrapper.h"
#include "interop/modules/common.h"
#include "interop/modules/imppg.h"
#include "interop/state.h"

namespace scripting::modules::imppg
{

const luaL_Reg functions[] = {
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
        CheckNumArgs(lua, "new_settings", 0);
        new(PrepareObject<SettingsWrapper>(lua)) SettingsWrapper();
        return 1;
    }},

    {"load_image", [](lua_State* lua) -> int {
        CheckNumArgs(lua, "load_image", 1);
        const std::string imagePath = GetString(lua, 1);
        new(PrepareObject<ImageWrapper>(lua)) ImageWrapper(imagePath);
        return 1;
    }},

    {"process_image_file", [](lua_State* lua) -> int {
        CheckNumArgs(lua, "process_image_file", 4);
        const std::string imagePath = GetString(lua, 1);
        const std::string settingsPath = GetString(lua, 2);
        const std::string outputImagePath = GetString(lua, 3);
        const int ofVal = GetInteger(lua, 4);

        if (ofVal < 0 || ofVal >= static_cast<int>(OutputFormat::LAST))
        {
            throw ScriptExecutionError{"invalid output format"};
        }

        scripting::g_State->CallFunctionAndAwaitCompletion(scripting::contents::ProcessImageFile{imagePath, settingsPath,
            outputImagePath, static_cast<OutputFormat>(ofVal)});

        return 0; //TODO: return processing result
    }},

    {"process_image", [](lua_State* lua) -> int {
        CheckNumArgs(lua, "process_image", 2);
        const auto image = GetObject<ImageWrapper>(lua, 1);
        const auto settings = GetObject<SettingsWrapper>(lua, 2);

        const auto result = scripting::g_State->CallFunctionAndAwaitCompletion(
            scripting::contents::ProcessImage{image.GetImage(), settings.GetSettings()}
        );
        const auto* processedImg = std::get_if<call_result::ImageProcessed>(&result);
        IMPPG_ASSERT(processedImg != nullptr);

        new(PrepareObject<ImageWrapper>(lua)) ImageWrapper(processedImg->image);

        return 1;
    }},

    {"progress", [](lua_State* lua) -> int {
        CheckNumArgs(lua, "progress", 1);
        const int percentage = GetInteger(lua, 1);
        scripting::g_State->SendMessage(scripting::contents::Progress{percentage});
        return 0;
    }},

    {nullptr, nullptr} // end-of-data marker
};

const std::vector<std::pair<std::string, int>> constants{
#if USE_FREEIMAGE
    {"BMP_8",        static_cast<int>(OutputFormat::BMP_8)},
    {"PNG_8",        static_cast<int>(OutputFormat::PNG_8)},
    {"TIFF_8_LZW",   static_cast<int>(OutputFormat::TIFF_8_LZW)},
    {"TIFF_16",      static_cast<int>(OutputFormat::TIFF_16)},
    {"TIFF_16_ZIP",  static_cast<int>(OutputFormat::TIFF_16_ZIP)},
    {"TIFF_32F",     static_cast<int>(OutputFormat::TIFF_32F)},
    {"TIFF_32F_ZIP", static_cast<int>(OutputFormat::TIFF_32F_ZIP)},
#else
    {"BMP_8",        static_cast<int>(OutputFormat::BMP_8)},
    {"TIFF_16",      static_cast<int>(OutputFormat::TIFF_16)},
#endif

#if USE_CFITSIO
    {"FITS_8",       static_cast<int>(OutputFormat::FITS_8)},
    {"FITS_16",      static_cast<int>(OutputFormat::FITS_16)},
    {"FITS_32F",     static_cast<int>(OutputFormat::FITS_32F)},
#endif
};

}
