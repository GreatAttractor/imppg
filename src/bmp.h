/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2017 Filip Szczerek <ga.software@yahoo.com>

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
    BMP-related functions header.
*/

#ifndef ImPPG_BMP_H
#define ImPPG_BMP_H

#include "image.h"

/// Reads a BMP image; returns 0 on error
c_Image *ReadBmp(const char *fileName);

/// Saves image in BMP format; returns 'false' on error
bool SaveBmp(const char *fileName, c_Image &img);

bool GetBmpDimensions(const char *fileName, unsigned &imgWidth, unsigned &imgHeight);

#endif // ImPPG_BMP_H
