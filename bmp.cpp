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
    BMP-related functions.
*/

#include <fstream>
#include <cassert>
#include <cstdlib>
#include <boost/cstdint.hpp>

#include "image.h"

bool IsMachineBigEndian();

#pragma pack(push)

#pragma pack(1)
typedef struct
{
    uint16_t bfType;
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER_t; ///< BMP file header

#pragma pack(1)
typedef struct
{
   uint32_t biSize;
   int32_t  biWidth;
   int32_t  biHeight;
   uint16_t biPlanes;
   uint16_t biBitCount;
   uint32_t biCompression;
   uint32_t biSizeImage;
   int32_t  biXPelsPerMeter;
   int32_t  biYPelsPerMeter;
   uint32_t biClrUsed;
   uint32_t biClrImportant;
} BITMAPINFOHEADER_t; ///< BMP info header

#pragma pack(pop)

const uint32_t BI_RGB = 0;
const uint32_t BI_BITFIELDS = 3;
const int BMP_PALETTE_SIZE = 256*4;

// returns the least multiple of 4 which is >= x
#define UP4MULT(x) (((x)+3)/4*4)

/// Reads a BMP image; returns 0 on error
c_Image *ReadBmp(const char *fileName)
{
    std::ifstream file(fileName, std::ios_base::in | std::ios_base::binary);
    if (file.fail())
        return 0;

    unsigned imgWidth, imgHeight;
    PixelFormat_t pixFmt = PIX_INVALID;

    bool isMBE = IsMachineBigEndian();

    BITMAPFILEHEADER_t bmpFileHdr;
    BITMAPINFOHEADER_t bmpInfoHdr;

    file.read((char *)&bmpFileHdr, sizeof(bmpFileHdr));
    file.read((char *)&bmpInfoHdr, sizeof(bmpInfoHdr));
    if (file.eof())
        return 0;

    // Fields in a BMP are always little-endian, so swap them if running on a big-endian machine

    int bitsPerPixel = SWAP16cnd(bmpInfoHdr.biBitCount, isMBE);

    imgWidth = SWAP32cnd(bmpInfoHdr.biWidth, isMBE);
    imgHeight = SWAP32cnd(bmpInfoHdr.biHeight, isMBE);
    if (imgWidth == 0 || imgHeight == 0 ||
        SWAP16cnd(bmpFileHdr.bfType, isMBE) != 'B'+((int)'M'<<8) ||
        SWAP16cnd(bmpInfoHdr.biPlanes, isMBE) != 1 ||
        bitsPerPixel != 8 && bitsPerPixel != 24 && bitsPerPixel != 32 ||
        SWAP32cnd(bmpInfoHdr.biCompression, isMBE) != BI_RGB && SWAP32cnd(bmpInfoHdr.biCompression, isMBE) != BI_BITFIELDS)
    {
        return 0;
    }

    int srcBytesPerPixel;

    if (bitsPerPixel == 8)
        pixFmt = PIX_PAL8;
    else if (bitsPerPixel == 24 || bitsPerPixel == 32)
    {
        pixFmt = PIX_RGB8;
        srcBytesPerPixel = bitsPerPixel/8;
    }

    c_Image *img = new c_Image(imgWidth, imgHeight, pixFmt);

    if (pixFmt == PIX_PAL8)
    {
        unsigned bmpStride = UP4MULT(imgWidth); // line length in bytes in the BMP file's pixel data
        unsigned skip = bmpStride - imgWidth; // number of padding bytes at the end of a line

        int numUsedPalEntries = SWAP32cnd(bmpInfoHdr.biClrUsed, isMBE);
        if (numUsedPalEntries == 0)
            numUsedPalEntries = 256;

        // seek to the beginning of palette
        file.seekg(sizeof(bmpFileHdr) + SWAP32cnd(bmpInfoHdr.biSize, isMBE), std::ios_base::beg);

        uint8_t palette[BMP_PALETTE_SIZE]; /// BMP-style palette (B, G, R, pad)
        file.read((char *)palette, numUsedPalEntries*4);

        // Convert to RGB-order palette
        for (int i = 0; i < numUsedPalEntries; i++)
        {
            img->GetPalette()[3*i + 0] = palette[i*4 + 2];
            img->GetPalette()[3*i + 1] = palette[i*4 + 1];
            img->GetPalette()[3*i + 2] = palette[i*4 + 0];
        }

        // Seek to the beginning of pixel values
        file.seekg(SWAP32cnd(bmpFileHdr.bfOffBits, isMBE), std::ios_base::beg);

        for (int y = imgHeight - 1; y >= 0; y--) // lines in BMP are stored bottom to top
        {
            file.read((char *)((uint8_t *)img->GetRow(y)), imgWidth);
            if (skip > 0)
                file.seekg(skip, std::ios_base::cur);
        }
    }
    else if (pixFmt == PIX_RGB8)
    {
        unsigned bmpStride = UP4MULT(imgWidth*srcBytesPerPixel); // line length in bytes in the BMP file's pixel data
        unsigned skip = bmpStride - imgWidth*srcBytesPerPixel; // number of padding bytes at the end of a row

        // Seek to the beginning of pixel values
        file.seekg(SWAP32cnd(bmpFileHdr.bfOffBits, isMBE), std::ios_base::beg);

        uint8_t *row = new uint8_t[imgWidth*srcBytesPerPixel];

        // read the lines directly into the buffer
        for (int y = imgHeight - 1; y >= 0; y--) // lines in BMP are stored bottom to top
        {
            file.read((char *)row, imgWidth*srcBytesPerPixel);
            
            if (srcBytesPerPixel == 3)
            {
                for (int x = 0; x < imgWidth; x++)
                {
                    ((uint8_t *)img->GetRow(y))[x*3+0] = row[3*x + 2];
                    ((uint8_t *)img->GetRow(y))[x*3+1] = row[3*x + 1];
                    ((uint8_t *)img->GetRow(y))[x*3+2] = row[3*x + 0];
                }
            }
            else if (srcBytesPerPixel == 4)
            {
                // Remove the unused 4th byte from each pixel and rearrange the channels to RGB order
                for (int x = 0; x < imgWidth; x++)
                {
                    ((uint8_t *)img->GetRow(y))[x*3+0] = row[x*4+3];
                    ((uint8_t *)img->GetRow(y))[x*3+1] = row[x*4+2];
                    ((uint8_t *)img->GetRow(y))[x*3+2] = row[x*4+1];
                }
            }
            
            if (skip > 0)
                file.seekg(skip, std::ios_base::cur);
        }

        delete[] row;
    }

    return img;
}

/// Saves image in BMP format; returns 'false' on error
bool SaveBmp(const char *fileName, c_Image &img)
{
    PixelFormat_t pixFmt = img.GetPixelFormat();
    assert(pixFmt == PIX_PAL8 ||
           pixFmt == PIX_RGB8 ||
           pixFmt == PIX_MONO8);

    BITMAPFILEHEADER_t bmfh;
    BITMAPINFOHEADER_t bmih;
    int i;

    int bytesPP = BytesPerPixel[img.GetPixelFormat()];
    unsigned bmpLineWidth = UP4MULT(img.GetWidth() * bytesPP);

    // Fields in a BMP are always little-endian, so swap them if running on a big-endian machine
    bool isMBE = IsMachineBigEndian();

    bmfh.bfType = SWAP16cnd('B'+((int)'M'<<8), isMBE);
    bmfh.bfSize = sizeof(bmfh) + sizeof(bmih) + img.GetHeight()*bmpLineWidth;
    if (pixFmt == PIX_PAL8 || pixFmt == PIX_MONO8)
        bmfh.bfSize += BMP_PALETTE_SIZE;
    bmfh.bfSize = SWAP32cnd(bmfh.bfSize, isMBE);
    bmfh.bfReserved1 = 0;
    bmfh.bfReserved2 = 0;
    bmfh.bfOffBits = sizeof(bmih) + sizeof(bmfh);
    if (pixFmt == PIX_PAL8 || pixFmt == PIX_MONO8)
        bmfh.bfOffBits += BMP_PALETTE_SIZE;
    bmfh.bfOffBits = SWAP32cnd(bmfh.bfOffBits, isMBE);

    bmih.biSize = SWAP32cnd(sizeof(bmih), isMBE);
    bmih.biWidth = SWAP32cnd(img.GetWidth(), isMBE);
    bmih.biHeight = SWAP32cnd(img.GetHeight(), isMBE);
    bmih.biPlanes = SWAP16cnd(1, isMBE);
    bmih.biBitCount = SWAP16cnd(bytesPP * 8, isMBE);
    bmih.biCompression = SWAP32cnd(BI_RGB, isMBE);
    bmih.biSizeImage = 0;
    bmih.biXPelsPerMeter = SWAP32cnd(1000, isMBE);
    bmih.biYPelsPerMeter = SWAP32cnd(1000, isMBE);
    bmih.biClrUsed = 0;
    bmih.biClrImportant = 0;

    std::ofstream file(fileName, std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
    if (file.fail())
        return false;

    file.write((const char *)&bmfh, sizeof(bmfh));
    file.write((const char *)&bmih, sizeof(bmih));
    if (img.GetPixelFormat() == PIX_PAL8 || img.GetPixelFormat() == PIX_MONO8)
    {
        uint8_t palette[BMP_PALETTE_SIZE]; // BMP-style palette

        if (img.GetPixelFormat() == PIX_PAL8)
        {
            for (i = 0; i < 256; i++)
            {
                palette[4*i + 0] = img.GetPalette()[3*i + 2];
                palette[4*i + 1] = img.GetPalette()[3*i + 1];
                palette[4*i + 2] = img.GetPalette()[3*i + 0];
                palette[4*i + 3] = 0;
            }
        }
        else
        {
            for (i = 0; i < 256; i++)
            {
                palette[4*i + 0] = i;
                palette[4*i + 1] = i;
                palette[4*i + 2] = i;
                palette[4*i + 3] = 0;
            }
        }

        file.write((const char *)palette, BMP_PALETTE_SIZE);
    }

    int skip = bmpLineWidth - img.GetWidth()*bytesPP;

    uint8_t *row = 0;
    if (pixFmt == PIX_RGB8)
    {
        row = new uint8_t[img.GetWidth()*bytesPP];
    }
    
    for (i = img.GetHeight() - 1; i >= 0; i--) // lines in a BMP are stored bottom to top
    {
        if (pixFmt == PIX_RGB8)
        {
            for (int x = 0; x < img.GetWidth(); x++)
            {
                uint8_t temp = row[3*x + 0];
                row[3*x + 0] = row[3*x + 2];
                row[3*x + 2] = temp;
            }
            file.write((const char*)row, img.GetWidth()*bytesPP);
        }
        else
            file.write((const char*)img.GetRow(i), img.GetWidth()*bytesPP);
        
        if (skip > 0)
            file.write((const char *)img.GetRow(i), skip); //this is just padding, so write anything
    }
    
    delete[] row;

    file.close();

    return true;
}

bool GetBmpDimensions(const char *fileName, unsigned &imgWidth, unsigned &imgHeight)
{
    std::ifstream file(fileName, std::ios_base::in | std::ios_base::binary);
    if (file.fail())
        return false;

    BITMAPFILEHEADER_t bmpFileHdr;
    BITMAPINFOHEADER_t bmpInfoHdr;

    file.read((char *)&bmpFileHdr, sizeof(bmpFileHdr));
    file.read((char *)&bmpInfoHdr, sizeof(bmpInfoHdr));
    if (file.eof())
        return false;

    // Fields in a BMP are always little-endian, so swap them if running on a big-endian machine
    bool isMBE = IsMachineBigEndian();

    imgWidth = SWAP32cnd(bmpInfoHdr.biWidth, isMBE);
    imgHeight = SWAP32cnd(bmpInfoHdr.biHeight, isMBE);

    return true;
}
