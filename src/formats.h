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
    Supported file formats.
*/
#ifndef IMPPG_FORMATS_HEADER
#define IMPPG_FORMATS_HEADER

#include <string>
#include <wx/translation.h>

// Filter string used by the File Open (image) dialog
extern const char *INPUT_FILE_FILTERS;

/// Supported output file formats
enum class OutputFormat
{
#if USE_FREEIMAGE

    BMP_MONO_8 = 0,    ///< 8-bit mono BMP
    PNG_MONO_8,        ///< 8-bit mono PNG
    TIFF_MONO_8_LZW,   ///< 8-bit mono TIFF, LZW compression
    TIFF_MONO_16,      ///< 16-bit mono TIFF, no compression
    TIFF_MONO_16_ZIP,  ///< 16-bit mono TIFF, ZIP (Deflate) compression
    TIFF_MONO_32F,     ///< 32-bit floating-point mono TIFF, no compression
    TIFF_MONO_32F_ZIP, ///< 32-bit floating-point mono TIFF, ZIP (Deflate) compression

#else

    BMP_MONO_8 = 0, ///< 8-bit mono BMP
    TIFF_MONO_16,   ///< 16-bit mono TIFF, no compression

#endif

#if USE_CFITSIO
    FITS_8,
    FITS_16,
    FITS_32F,
#endif

    LAST // this has to be the last element
};

std::string GetOutputFormatDescription(OutputFormat fmt, std::string *wildcard = 0);

/// Returns output filters suitable for use in a File Save dialog
std::string GetOutputFilters();

#endif // IMPPG_FORMATS_HEADER
