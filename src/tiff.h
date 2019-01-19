/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2019 Filip Szczerek <ga.software@yahoo.com>

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

#include <string>
#include "image.h"

bool GetTiffDimensions(const char *fileName, unsigned &imgWidth, unsigned &imgHeight);

/// Reads a TIFF image; returns 0 on error
c_Image *ReadTiff(
    const char *fileName,
    std::string *errorMsg = 0 ///< If not null, receives error message (if any)
);

/// Saves image in TIFF format; returns 'false' on error
bool SaveTiff(const char *fileName, const c_Image &img);

#endif // ImPPG_TIFF_H
