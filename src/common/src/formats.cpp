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

#include <wx/string.h>

#include "common/formats.h"

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

wxString GetOutputFormatDescription(OutputFormat fmt, wxString* wildcard)
{
    switch (fmt)
    {
    case OutputFormat::BMP_8:
        if (wildcard)
            *wildcard = "*.bmp";
        return _("BMP 8-bit");

    case OutputFormat::TIFF_16:
        if (wildcard)
            *wildcard = "*.tif";
        return _("TIFF 16-bit");

#if (USE_FREEIMAGE)

    case OutputFormat::PNG_8:
        if (wildcard)
            *wildcard = "*.png";
        return _("PNG 8-bit");

    case OutputFormat::TIFF_8_LZW:
        if (wildcard)
            *wildcard = "*.tif";
        return _("TIFF 8-bit (LZW compression)");

    case OutputFormat::TIFF_16_ZIP:
        if (wildcard)
            *wildcard = "*.tif";
        return _("TIFF 16-bit (ZIP compression)");

    case OutputFormat::TIFF_32F:
        if (wildcard)
            *wildcard = "*.tif";
        return _("TIFF 32-bit floating-point");

    case OutputFormat::TIFF_32F_ZIP:
        if (wildcard)
            *wildcard = "*.tif";
        return _("TIFF 32-bit floating-point (ZIP compression)");

#endif

#if USE_CFITSIO
    case OutputFormat::FITS_8:
        if (wildcard)
            *wildcard = "*.fit";
        return _("FITS 8-bit");

    case OutputFormat::FITS_16:
        if (wildcard)
            *wildcard = "*.fit";
        return _("FITS 16-bit");

    case OutputFormat::FITS_32F:
        if (wildcard)
            *wildcard = "*.fit";
        return _("FITS 32-bit floating point");
#endif

    default: return "";
    }
}

/// Returns output filters suitable for use in a File Save dialog
wxString GetOutputFilters()
{
    wxString filters;

    for (int i = 0; i < static_cast<int>(OutputFormat::LAST); i++)
    {
        if (i > 0)
            filters += "|";

        wxString wildcard;
        filters += GetOutputFormatDescription(static_cast<OutputFormat>(i), &wildcard);
        filters += "|" + wildcard;
    }

    return filters;
}
