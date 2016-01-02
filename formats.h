/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2015, 2016 Filip Szczerek <ga.software@yahoo.com>

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
typedef enum
{
#if USE_FREEIMAGE

    OUTF_BMP_MONO_8 = 0,    ///< 8-bit mono BMP
    OUTF_PNG_MONO_8,        ///< 8-bit mono PNG
    OUTF_TIFF_MONO_8_LZW,   ///< 8-bit mono TIFF, LZW compression
    OUTF_TIFF_MONO_16,      ///< 16-bit mono TIFF, no compression
    OUTF_TIFF_MONO_16_ZIP,  ///< 16-bit mono TIFF, ZIP (Deflate) compression
    OUTF_TIFF_MONO_32F,     ///< 32-bit floating-point mono TIFF, no compression
    OUTF_TIFF_MONO_32F_ZIP, ///< 32-bit floating-point mono TIFF, ZIP (Deflate) compression

#else

    OUTF_BMP_MONO_8 = 0, ///< 8-bit mono BMP
    OUTF_TIFF_MONO_16,   ///< 16-bit mono TIFF, no compression

#endif

#if USE_CFITSIO
    OUTF_FITS_8,
    OUTF_FITS_16,
    OUTF_FITS_32F,
#endif

    OUTF_LAST // this has to be the last element
} OutputFormat_t;

std::string GetOutputFormatDescription(OutputFormat_t fmt, std::string *wildcard = 0);

/// Returns output filters suitable for use in a File Save dialog
std::string GetOutputFilters();

#endif // IMPPG_FORMATS_HEADER
