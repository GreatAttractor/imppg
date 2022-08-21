#pragma once

#include "image/image.h"

#include <lua.hpp>
#include <optional>
#include <string>

namespace scripting
{

class ImageWrapper
{
public:
    ImageWrapper(const std::string& imagePath);

    static const luaL_Reg* GetMethods()
    {
        static const luaL_Reg methods[] = {

            {nullptr, nullptr} // end-of-data marker
        };

        return methods;
    }

    const c_Image& GetImage() const;

private:
    // optional used for delayed construction
    std::optional<c_Image> m_Image;
};

}
