#pragma once

#include "image/image.h"
#include "interop/classes/method.h"

#include <lua.hpp>
#include <memory>
#include <optional>
#include <string>

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

            {nullptr, nullptr} // end-of-data marker
        };

        return methods;
    }

    const std::shared_ptr<const c_Image>& GetImage() const;

    void save(const std::string& path, int outputFormat) const;

private:
    std::shared_ptr<const c_Image> m_Image;
};

}
