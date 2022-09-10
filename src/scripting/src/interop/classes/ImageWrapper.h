#pragma once

#include "image/image.h"

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

    static const luaL_Reg* GetMethods()
    {
        static const luaL_Reg methods[] = {

            {nullptr, nullptr} // end-of-data marker
        };

        return methods;
    }

    const std::shared_ptr<const c_Image>& GetImage() const;

private:
    std::shared_ptr<const c_Image> m_Image;
};

}
