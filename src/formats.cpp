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

#include <wx/string.h>
#include "formats.h"

// Filter string used by the File Open (image) dialog
const char *INPUT_FILE_FILTERS =

    "*.*|*.*"
    "|BMP (*.bmp)|*.bmp"
    "|TIFF (*.tif)|*.tif;*.tiff"

#if USE_FREEIMAGE
    // All file types supported by FreeImage can be loaded, but we do not list them all as filters
    "|JPEG (*.jpg)|*.jpg"
    "|PNG (*.png)|*.png"
#endif

#if USE_CFITSIO
    "|FITS (*.fit)|*.fit;*.fits"
#endif

;

std::string GetOutputFormatDescription(OutputFormat_t fmt, std::string *wildcard)
{
    switch (fmt)
    {
    case OUTF_BMP_MONO_8:
        if (wildcard)
            *wildcard = "*.bmp";
        return std::string(_("BMP 8-bit"));

    case OUTF_TIFF_MONO_16:
        if (wildcard)
            *wildcard = "*.tif";
        return std::string(_("TIFF 16-bit"));

#if (USE_FREEIMAGE)

    case OUTF_PNG_MONO_8:
        if (wildcard)
            *wildcard = "*.png";
        return std::string(_("PNG 8-bit"));

    case OUTF_TIFF_MONO_8_LZW:
        if (wildcard)
            *wildcard = "*.tif";
        return std::string(_("TIFF 8-bit (LZW compression)"));

    case OUTF_TIFF_MONO_16_ZIP:
        if (wildcard)
            *wildcard = "*.tif";
        return std::string(_("TIFF 16-bit (ZIP compression)"));

    case OUTF_TIFF_MONO_32F:
        if (wildcard)
            *wildcard = "*.tif";
        return std::string(_("TIFF 32-bit floating-point"));

    case OUTF_TIFF_MONO_32F_ZIP:
        if (wildcard)
            *wildcard = "*.tif";
        return std::string(_("TIFF 32-bit floating-point (ZIP compression)"));

#endif

#if USE_CFITSIO
    case OUTF_FITS_8:
        if (wildcard)
            *wildcard = "*.fit";
        return std::string(_("FITS 8-bit"));

    case OUTF_FITS_16:
        if (wildcard)
            *wildcard = "*.fit";
        return std::string(_("FITS 16-bit"));

    case OUTF_FITS_32F:
        if (wildcard)
            *wildcard = "*.fit";
        return std::string(_("FITS 32-bit floating point"));
#endif

    default: return "";
    }
}

/// Returns output filters suitable for use in a File Save dialog
std::string GetOutputFilters()
{
    std::string filters;

    for (int i = 0; i < OUTF_LAST; i++)
    {
        if (i > 0)
            filters += "|";

        std::string wildcard;
        filters += GetOutputFormatDescription((OutputFormat_t)i, &wildcard);
        filters += "|" + wildcard;
    }

    return filters;
}
