/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2023-2025 Filip Szczerek <ga.software@yahoo.com>

This file is part of ImPPG.

ImPPG is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

ImPPG is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with ImPPG.  If not, see <http://www.gnu.org/licenses/>.

File description:
    Image wrapper header.
*/

#pragma once

#include "interop/classes/method.h"

#include <filesystem>
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
    static ImageWrapper FromPath(const std::filesystem::path& imagePath);

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

    void save(const wxString& path, int outputFormat) const;

    void align_rgb();

    void auto_white_balance();

    void multiply(double factor);

private:
    ImageWrapper(const std::filesystem::path& imagePath);

    std::shared_ptr<const c_Image> m_Image;
};

}
