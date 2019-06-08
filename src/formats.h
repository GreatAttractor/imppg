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
    Supported file formats.
*/
#ifndef IMPPG_FORMATS_HEADER
#define IMPPG_FORMATS_HEADER

#include <string>
#include <wx/translation.h>

// Filter string used by the File Open (image) dialog
extern const char* INPUT_FILE_FILTERS;

/// Supported output file formats
enum class OutputFormat
{
#if USE_FREEIMAGE

    BMP_8 = 0,    ///< 8-bit mono BMP
    PNG_8,        ///< 8-bit mono PNG
    TIFF_8_LZW,   ///< 8-bit mono TIFF, LZW compression
    TIFF_16,      ///< 16-bit mono TIFF, no compression
    TIFF_16_ZIP,  ///< 16-bit mono TIFF, ZIP (Deflate) compression
    TIFF_32F,     ///< 32-bit floating-point mono TIFF, no compression
    TIFF_32F_ZIP, ///< 32-bit floating-point mono TIFF, ZIP (Deflate) compression

#else

    BMP_8 = 0, ///< 8-bit mono BMP
    TIFF_16,   ///< 16-bit mono TIFF, no compression

#endif

#if USE_CFITSIO
    FITS_8,
    FITS_16,
    FITS_32F,
#endif

    LAST // has to be the last element
};

enum class OutputBitDepth
{
    Uint8,
    Uint16,
#if USE_FREEIMAGE || USE_CFITSIO
    Float32,
#endif
    Unchanged
};

enum class OutputFileType
{
    BMP,
    TIFF,           ///< Uncompressed
#if USE_FREEIMAGE
    PNG,
    TIFF_COMPR_LZW, ///< LZW compression
    TIFF_COMPR_ZIP, ///< ZIP (Deflate) compression
#endif
#if USE_CFITSIO
    FITS
#endif
};

std::string GetOutputFormatDescription(OutputFormat fmt, std::string* wildcard = nullptr);

/// Returns output filters suitable for use in a File Save dialog
std::string GetOutputFilters();

#endif // IMPPG_FORMATS_HEADER
