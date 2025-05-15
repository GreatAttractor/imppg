#pragma once

#include "interop/classes/method.h"

#include <lua.hpp>
#include <memory>
#include <optional>
#include <string>

class c_Image;

namespace scripting
{

class ImageWrapper
{
public:
    ImageWrapper(const std::string& imagePath);

    ImageWrapper(const std::shared_ptr<const c_Image>& image);

    ImageWrapper(c_Image&& image);

    static const luaL_Reg* GetMethods()
    {
        static const luaL_Reg methods[] = {
            {"save", [](lua_State* lua) {
                return ConstMethodStringIntArg<ImageWrapper>(lua, &ImageWrapper::save);
            }},

            {"align_rgb", [](lua_State* lua) -> int {
                return MethodNoResult<ImageWrapper>(lua, &ImageWrapper::align_rgb);
            }},

            {"awb", [](lua_State* lua) -> int {
                return MethodNoResult<ImageWrapper>(lua, &ImageWrapper::auto_white_balance);
            }},

            {"multiply", [](lua_State* lua) -> int {
                return MethodDoubleArg<ImageWrapper>(lua, &ImageWrapper::multiply);
            }},

            {nullptr, nullptr} // end-of-data marker
        };

        return methods;
    }

    const std::shared_ptr<const c_Image>& GetImage() const;

    void save(const std::string& path, int outputFormat) const;

    void align_rgb();

    void auto_white_balance();

    void multiply(double factor);

private:
    std::shared_ptr<const c_Image> m_Image;
};

}
