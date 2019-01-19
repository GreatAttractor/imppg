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
    Cursors definition.
*/

#include <wx/cursor.h>
#include <wx/bitmap.h>

// converts a 8-digit binary string into unsigned char value; NOTE: char[0] is the least significant bit
#define BITM(str) (char)(((str[0]-'0') << 0) + \
                         ((str[1]-'0') << 1) + \
                         ((str[2]-'0') << 2) + \
                         ((str[3]-'0') << 3) + \
                         ((str[4]-'0') << 4) + \
                         ((str[5]-'0') << 5) + \
                         ((str[6]-'0') << 6) + \
                         ((str[7]-'0') << 7))

namespace Cursors
{
    wxCursor crHandOpen, crHandClosed;

    void negateBits(char array[], int length)
    {
        for (int i = 0; i < length; i++)
            array[i] = ~array[i];
    }

    void InitCursors()
    {
        // Cursor images are in XBM format, i.e. subsequent pixels in a row
        // are defined by subsequent bits, starting with the least significant bit.
        // Thanks to BITM() macro the strings below may be viewed directly as pixel arrays.

        // bit 1: white, bit 0: black
        static const char crHandOpenBits[] = {
            BITM("00000011"), BITM("10000000"),
            BITM("00001101"), BITM("10000000"),
            BITM("00110100"), BITM("10000000"),
            BITM("00100110"), BITM("11000000"),
            BITM("01010010"), BITM("11000000"),
            BITM("01011011"), BITM("01000000"),
            BITM("01001001"), BITM("01100000"),
            BITM("10101001"), BITM("01100011"),
            BITM("10100100"), BITM("00100111"),
            BITM("10010010"), BITM("00111001"),
            BITM("11001010"), BITM("00110011"),
            BITM("01100000"), BITM("00000110"),
            BITM("01100000"), BITM("00000110"),
            BITM("00110000"), BITM("00000100"),
            BITM("00011000"), BITM("00000100"),
            BITM("00001000"), BITM("00011100")
        };

        // bit 0: opaque, bit 1: transparent
        static char crHandOpenMask[16*16] = {
            BITM("11111100"), BITM("01111111"),
            BITM("11110000"), BITM("01111111"),
            BITM("11000000"), BITM("01111111"),
            BITM("11000000"), BITM("00111111"),
            BITM("10000000"), BITM("00111111"),
            BITM("10000000"), BITM("00111111"),
            BITM("10000000"), BITM("00011111"),
            BITM("00000000"), BITM("00011100"),
            BITM("00000000"), BITM("00011000"),
            BITM("00000000"), BITM("00000000"),
            BITM("00000000"), BITM("00000000"),
            BITM("10000000"), BITM("00000001"),
            BITM("10000000"), BITM("00000001"),
            BITM("11000000"), BITM("00000011"),
            BITM("11100000"), BITM("00000011"),
            BITM("11110000"), BITM("00000011")
        };

#ifdef __WXMSW__
        wxBitmap bmpHandOpen((const char *)crHandOpenBits, 16, 16);
        wxBitmap bmpHandOpenMask((const char*)crHandOpenMask, 16, 16);
        bmpHandOpen.SetMask(new wxMask(bmpHandOpenMask));
        wxImage imgHandOpen = bmpHandOpen.ConvertToImage();
        imgHandOpen.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 7);
        imgHandOpen.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 7);
        crHandOpen = wxCursor(imgHandOpen);
#else
        negateBits(crHandOpenMask, sizeof(crHandOpenMask));
        crHandOpen = wxCursor(crHandOpenBits, 16, 16, 7, 7,
            crHandOpenMask, wxWHITE, wxBLACK);
#endif

        static const char crHandClosedBits[] = {
            BITM("00000000"), BITM("00000000"),
            BITM("00000000"), BITM("00000000"),
            BITM("00000000"), BITM("00000000"),
            BITM("00000111"), BITM("11000000"),
            BITM("00011100"), BITM("01100000"),
            BITM("01110000"), BITM("00111000"),
            BITM("01000000"), BITM("00011100"),
            BITM("01000000"), BITM("00010110"),
            BITM("01000000"), BITM("00010010"),
            BITM("01000000"), BITM("00010010"),
            BITM("01000000"), BITM("00001010"),
            BITM("01000000"), BITM("00000010"),
            BITM("01100000"), BITM("00000110"),
            BITM("00110000"), BITM("00000100"),
            BITM("00011000"), BITM("00000100"),
            BITM("00001000"), BITM("00001100")
        };

        // bit 0: opaque, bit 1: transparent
        static char crHandClosedMask[16*16] = {
            BITM("11111111"), BITM("11111111"),
            BITM("11111111"), BITM("11111111"),
            BITM("11111111"), BITM("11111111"),
            BITM("11111000"), BITM("00111111"),
            BITM("11100000"), BITM("00011111"),
            BITM("10000000"), BITM("00000111"),
            BITM("10000000"), BITM("00000011"),
            BITM("10000000"), BITM("00000001"),
            BITM("10000000"), BITM("00000001"),
            BITM("10000000"), BITM("00000001"),
            BITM("10000000"), BITM("00000001"),
            BITM("10000000"), BITM("00000001"),
            BITM("10000000"), BITM("00000001"),
            BITM("11000000"), BITM("00000011"),
            BITM("11100000"), BITM("00000011"),
            BITM("11110000"), BITM("00000011")
        };

#ifdef __WXMSW__
        wxBitmap bmpHandClosed((const char *)crHandClosedBits, 16, 16);
        wxBitmap bmpHandClosedMask((const char*)crHandClosedMask, 16, 16);
        bmpHandClosed.SetMask(new wxMask(bmpHandClosedMask));
        wxImage imgHandClosed = bmpHandClosed.ConvertToImage();
        imgHandClosed.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_X, 7);
        imgHandClosed.SetOption(wxIMAGE_OPTION_CUR_HOTSPOT_Y, 7);
        crHandClosed = wxCursor(imgHandClosed);
#else
        negateBits(crHandClosedMask, sizeof(crHandClosedMask));
        crHandClosed = wxCursor(crHandClosedBits, 16, 16, 7, 7,
            crHandClosedMask, wxWHITE, wxBLACK);
#endif
    }

}
