/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2025 Filip Szczerek <ga.software@yahoo.com>

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
    TIFF-related functions header.
*/

#ifndef ImPPG_TIFF_H
#define ImPPG_TIFF_H

#include <filesystem>
#include <optional>
#include <string>

#include "image/image.h"

/// Returns (width, height).
std::optional<std::tuple<unsigned, unsigned>> GetTiffDimensions(const std::filesystem::path& fileName);

std::optional<c_Image> ReadTiff(
    const std::filesystem::path& fileName,
    std::string* errorMsg = nullptr ///< If not null, receives error message (if any)
);

/// Returns `false` on error.
bool SaveTiff(const std::filesystem::path& fileName, const IImageBuffer& img);

#endif // ImPPG_TIFF_H
