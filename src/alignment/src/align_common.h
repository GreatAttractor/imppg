/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2022 Filip Szczerek <ga.software@yahoo.com>

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
    Common code for image alignment.
*/

#pragma once

#include "image/image.h"

#include <optional>

/// Accessor for an owned or non-owned image; may be empty.
class ImageAccessor
{
public:
    ImageAccessor() = default;

    ImageAccessor(c_Image&& image): m_Owned{std::move(image)} {}
    ImageAccessor(const c_Image* image): m_NonOwned{image} {}

    ImageAccessor(const ImageAccessor&) = delete;
    ImageAccessor& operator=(const ImageAccessor&) = delete;

    ImageAccessor(ImageAccessor&&) = default;
    ImageAccessor& operator=(ImageAccessor&&) = default;

    ~ImageAccessor() = default;

    bool Empty() const { return !m_Owned.has_value() && nullptr == m_NonOwned; }

    const c_Image* Get() const
    {
        if (m_Owned.has_value())
        {
            return &*m_Owned;
        }
        else
        {
            return m_NonOwned;
        }
    }

private:
    std::optional<c_Image> m_Owned;
    const c_Image* m_NonOwned{nullptr};
};
