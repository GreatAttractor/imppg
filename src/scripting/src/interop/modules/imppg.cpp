#include "alignment/align_proc.h"
#include "common/formats.h"
#include "interop/classes/DummyObject1.h"
#include "interop/classes/DummyObject2.h"
#include "interop/classes/ImageWrapper.h"
#include "interop/classes/SettingsWrapper.h"
#include "interop/modules/common.h"
#include "interop/modules/imppg.h"
#include "interop/state.h"

#include <boost/format.hpp>
#include <filesystem>

namespace fs = std::filesystem;

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
        if (scripting::g_State->CheckStopRequested(lua)) { return 0; }

        CheckNumArgs(lua, "new_settings", 0);
        new(PrepareObject<SettingsWrapper>(lua)) SettingsWrapper();
        return 1;
    }},

    {"load_settings", [](lua_State* lua) -> int {
        if (scripting::g_State->CheckStopRequested(lua)) { return 0; }

        CheckNumArgs(lua, "load_settings", 1);
        const std::string path = GetString(lua, 1);
        new(PrepareObject<SettingsWrapper>(lua)) SettingsWrapper(path);
        return 1;
    }},

    {"load_image", [](lua_State* lua) -> int {
        if (scripting::g_State->CheckStopRequested(lua)) { return 0; }

        CheckNumArgs(lua, "load_image", 1);
        const std::string imagePath = GetString(lua, 1);
        new(PrepareObject<ImageWrapper>(lua)) ImageWrapper(imagePath);
        return 1;
    }},

    //TODO: rename?
    {"load_image_split_rgb", [](lua_State* lua) -> int {
        if (scripting::g_State->CheckStopRequested(lua)) { return 0; }

        CheckNumArgs(lua, "load_image_split_rgb", 1);

        const std::string imagePath = GetString(lua, 1);
        std::string internalErrorMsg;
        const auto image = LoadImage(imagePath, std::nullopt, &internalErrorMsg);
        if (!image.has_value())
        {
            auto message = std::string{"failed to load image from "} + imagePath;
            if (!internalErrorMsg.empty()) { message += "; " + internalErrorMsg; }
            throw ScriptExecutionError(message);
        }

        if (NumChannels[static_cast<int>(image->GetPixelFormat())] != 3)
        {
            throw ScriptExecutionError(boost::str(boost::format("\"%s\" is not an RGB image") % imagePath));
        }

        auto [red, green, blue] = image->SplitRGB();

        for (c_Image* channel: {&red, &green, &blue})
        {
            *channel = channel->ConvertPixelFormat(PixelFormat::PIX_MONO32F);
        }

        new(PrepareObject<ImageWrapper>(lua)) ImageWrapper(std::move(red));
        new(PrepareObject<ImageWrapper>(lua)) ImageWrapper(std::move(green));
        new(PrepareObject<ImageWrapper>(lua)) ImageWrapper(std::move(blue));
        return 3;
    }},

    {"process_image_file", [](lua_State* lua) -> int {
        if (scripting::g_State->CheckStopRequested(lua)) { return 0; }

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
        if (scripting::g_State->CheckStopRequested(lua)) { return 0; }

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
        if (scripting::g_State->CheckStopRequested(lua)) { return 0; }

        CheckNumArgs(lua, "progress", 1);
        const double fraction = GetNumber(lua, 1);
        scripting::g_State->SendMessage(scripting::contents::Progress{fraction});
        return 0;
    }},

    {"combine_rgb", [](lua_State* lua) -> int {
        if (scripting::g_State->CheckStopRequested(lua)) { return 0; }

        CheckNumArgs(lua, "combine_rgb", 3);
        const auto red = GetObject<ImageWrapper>(lua, 1);
        const auto green = GetObject<ImageWrapper>(lua, 2);
        const auto blue = GetObject<ImageWrapper>(lua, 3);
        new(PrepareObject<ImageWrapper>(lua)) ImageWrapper(
            c_Image::CombineRGB(*red.GetImage(), *green.GetImage(), *blue.GetImage())
        );
        return 1;
    }},

    //TODO: accept list of images and weights (tuples)
    {"blend", [](lua_State* lua) -> int {
        if (scripting::g_State->CheckStopRequested(lua)) { return 0; }

        CheckNumArgs(lua, "blend", 4);
        const auto img1 = GetObject<ImageWrapper>(lua, 1);
        const auto weight1 = GetNumber(lua, 2);
        const auto img2 = GetObject<ImageWrapper>(lua, 3);
        const auto weight2 = GetNumber(lua, 4);

        if (weight1 < 0.0 || weight1 > 1.0 || weight2 < 0.0 || weight2 > 1.0)
        {
            throw ScriptExecutionError{"invalid blend weight(s)"};
        }
        //TODO: check sizes, num. channels

        new(PrepareObject<ImageWrapper>(lua)) ImageWrapper(
            c_Image::Blend(*img1.GetImage(), weight1, *img2.GetImage(), weight2)
        );
        return 1;
    }},

    {"align_images", [](lua_State* lua) -> int {
        if (scripting::g_State->CheckStopRequested(lua)) { return 0; }

        CheckNumArgs(lua, "align_images", 7);
        std::vector<fs::path> inputFiles;
        for (const auto& s: GetStringTable(lua, 1))
        {
            inputFiles.push_back(s);
        }

        const AlignmentMethod alignMode = [&]() {
            const int value = GetInteger(lua, 2);
            if (value < 0 || value >= static_cast<int>(AlignmentMethod::NUM))
            {
                throw ScriptExecutionError{"invalid alignment mode"};
            }
            return static_cast<AlignmentMethod>(value);
        }();

        const CropMode cropMode = [&]() {
            const int value = GetInteger(lua, 3);
            if (value < 0 || value >= static_cast<int>(CropMode::NUM))
            {
                throw ScriptExecutionError{"invalid crop mode"};
            }
            return static_cast<CropMode>(value);
        }();

        const bool subpixelAlignment = GetBoolean(lua, 4);

        const fs::path outputDir = GetString(lua, 5);

        CheckType(lua, 6, LUA_TSTRING, true);
        const auto outputFNameSuffix = lua_isnil(lua, 6)
            ? std::nullopt
            : std::optional<std::string>{GetString(lua, 6)};

        CheckType(lua, 7, LUA_TFUNCTION, true);
        const auto progressCallback = lua_isnil(lua, 7)
            ? std::function<void(double)>{}
            : [lua](double value) {
                lua_pushvalue(lua, 7);
                lua_pushnumber(lua, value);
                lua_call(lua, 1, 0);
            };

        scripting::g_State->CallFunctionAndAwaitCompletion(scripting::contents::AlignImages{
            std::move(inputFiles),
            alignMode,
            cropMode,
            subpixelAlignment,
            outputDir,
            outputFNameSuffix,
            std::move(progressCallback)
        });

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

    {"STANDARD",   static_cast<int>(AlignmentMethod::PHASE_CORRELATION)},
    {"SOLAR_LIMB", static_cast<int>(AlignmentMethod::LIMB)},

    {"CROP",         static_cast<int>(CropMode::CROP_TO_INTERSECTION)},
    {"PAD",          static_cast<int>(CropMode::PAD_TO_BOUNDING_BOX)}
};

}
