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
    BMP-related functions.
*/

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <optional>

#include "image/image.h"
#include "common/imppg_assert.h"

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
#define UP4MULT(x) (((x) + 3) / 4 * 4)

/// Reads a BMP image; returns 0 on error
std::optional<c_Image> ReadBmp(const std::filesystem::path& fileName)
{
    std::ifstream file(fileName, std::ios_base::in | std::ios_base::binary);
    if (file.fail())
        return std::nullopt;

    PixelFormat pixFmt{};

    const bool isMBE = IsMachineBigEndian();

    BITMAPFILEHEADER_t bmpFileHdr;
    BITMAPINFOHEADER_t bmpInfoHdr;

    file.read(reinterpret_cast<char*>(&bmpFileHdr), sizeof(bmpFileHdr));
    file.read(reinterpret_cast<char*>(&bmpInfoHdr), sizeof(bmpInfoHdr));
    if (file.eof())
        return std::nullopt;

    // fields in a BMP are always little-endian, so swap them if running on a big-endian machine

    const int bitsPerPixel = SWAP16cnd(bmpInfoHdr.biBitCount, isMBE);

    const unsigned imgWidth = SWAP32cnd(bmpInfoHdr.biWidth, isMBE);
    const unsigned imgHeight = SWAP32cnd(bmpInfoHdr.biHeight, isMBE);
    if (imgWidth == 0 || imgHeight == 0 ||
        SWAP16cnd(bmpFileHdr.bfType, isMBE) != 'B'+(static_cast<int>('M')<<8) ||
        SWAP16cnd(bmpInfoHdr.biPlanes, isMBE) != 1 ||
        bitsPerPixel != 8 && bitsPerPixel != 24 && bitsPerPixel != 32 ||
        SWAP32cnd(bmpInfoHdr.biCompression, isMBE) != BI_RGB && SWAP32cnd(bmpInfoHdr.biCompression, isMBE) != BI_BITFIELDS)
    {
        return std::nullopt;
    }

    const int srcBytesPerPixel = bitsPerPixel / 8;

    if (bitsPerPixel == 8)
        pixFmt = PixelFormat::PIX_PAL8;
    else if (bitsPerPixel == 24 || bitsPerPixel == 32)
        pixFmt = PixelFormat::PIX_RGB8;

    std::optional<c_Image> img;

    if (pixFmt == PixelFormat::PIX_PAL8)
    {
        const unsigned bmpStride = UP4MULT(imgWidth); // line length in bytes in the BMP file's pixel data
        const unsigned skip = bmpStride - imgWidth; // number of padding bytes at the end of a line

        int numUsedPalEntries = SWAP32cnd(bmpInfoHdr.biClrUsed, isMBE);
        if (numUsedPalEntries == 0)
            numUsedPalEntries = 256;

        // seek to the beginning of palette
        file.seekg(sizeof(bmpFileHdr) + SWAP32cnd(bmpInfoHdr.biSize, isMBE), std::ios_base::beg);

        uint8_t palette[BMP_PALETTE_SIZE]; /// BMP-style palette (B, G, R, pad)
        file.read(reinterpret_cast<char*>(palette), numUsedPalEntries*4);

        bool isMono8 = true;
        if (numUsedPalEntries == 256)
        {
            for (int i = 0; i < 256; i++)
                if (palette[i*4 + 0] != palette[i*4 + 1] ||
                    palette[i*4 + 1] != palette[i*4 + 2] ||
                    palette[i*4 + 0] != i)
                {
                    isMono8 = false;
                    break;
                }
        }

        if (isMono8)
            pixFmt = PixelFormat::PIX_MONO8;

        img = c_Image(imgWidth, imgHeight, pixFmt);

        if (!isMono8)
        {
            // convert to RGB-order palette
            auto& buf = img->GetBuffer();
            for (int i = 0; i < numUsedPalEntries; i++)
            {
                buf.GetPalette()[3*i + 0] = palette[i*4 + 2];
                buf.GetPalette()[3*i + 1] = palette[i*4 + 1];
                buf.GetPalette()[3*i + 2] = palette[i*4 + 0];
            }
        }

        // seek to the beginning of pixel values
        file.seekg(SWAP32cnd(bmpFileHdr.bfOffBits, isMBE), std::ios_base::beg);

        for (int y = imgHeight - 1; y >= 0; y--) // lines in BMP are stored bottom to top
        {
            file.read(img.value().GetRowAs<char>(y), imgWidth);
            if (skip > 0)
                file.seekg(skip, std::ios_base::cur);
        }
    }
    else if (pixFmt == PixelFormat::PIX_RGB8)
    {
        img = c_Image(imgWidth, imgHeight, pixFmt);

        unsigned bmpStride = UP4MULT(imgWidth * srcBytesPerPixel); // line length in bytes in the BMP file's pixel data
        unsigned skip = bmpStride - imgWidth * srcBytesPerPixel; // number of padding bytes at the end of a row

        // seek to the beginning of pixel values
        file.seekg(SWAP32cnd(bmpFileHdr.bfOffBits, isMBE), std::ios_base::beg);

        std::unique_ptr<uint8_t[]> row(new uint8_t[imgWidth * srcBytesPerPixel]);

        // read the lines directly into the buffer
        for (int y = static_cast<int>(imgHeight) - 1; y >= 0; y--) // lines in BMP are stored bottom to top
        {
            file.read(reinterpret_cast<char*>(row.get()), imgWidth * srcBytesPerPixel);

            if (srcBytesPerPixel == 3)
            {
                for (unsigned x = 0; x < imgWidth; x++)
                {
                    img->GetRowAs<uint8_t>(y)[x * 3 + 0] = row[3 * x + 2];
                    img->GetRowAs<uint8_t>(y)[x * 3 + 1] = row[3 * x + 1];
                    img->GetRowAs<uint8_t>(y)[x * 3 + 2] = row[3 * x + 0];
                }
            }
            else if (srcBytesPerPixel == 4)
            {
                // remove the unused 4th byte from each pixel and rearrange the channels to RGB order
                for (unsigned x = 0; x < imgWidth; x++)
                {
                    img->GetRowAs<uint8_t>(y)[x * 3 + 0] = row[x * 4 + 3];
                    img->GetRowAs<uint8_t>(y)[x * 3 + 1] = row[x * 4 + 2];
                    img->GetRowAs<uint8_t>(y)[x * 3 + 2] = row[x * 4 + 1];
                }
            }

            if (skip > 0)
                file.seekg(skip, std::ios_base::cur);
        }
    }

    return img;
}

bool SaveBmp(const std::filesystem::path& fileName, const IImageBuffer& img)
{
    const PixelFormat pixFmt = img.GetPixelFormat();
    IMPPG_ASSERT(pixFmt == PixelFormat::PIX_PAL8 ||
                 pixFmt == PixelFormat::PIX_RGB8 ||
                 pixFmt == PixelFormat::PIX_MONO8);

    BITMAPFILEHEADER_t bmfh;
    BITMAPINFOHEADER_t bmih;
    int i;

    const int bytesPP = img.GetBytesPerPixel();
    const unsigned bmpLineWidth = UP4MULT(img.GetWidth() * bytesPP);

    // fields in a BMP are always little-endian, so swap them if running on a big-endian machine
    const bool isMBE = IsMachineBigEndian();

    bmfh.bfType = SWAP16cnd('B'+(static_cast<int>('M')<<8), isMBE);
    bmfh.bfSize = sizeof(bmfh) + sizeof(bmih) + img.GetHeight() * bmpLineWidth;
    if (pixFmt == PixelFormat::PIX_PAL8 || pixFmt == PixelFormat::PIX_MONO8)
        bmfh.bfSize += BMP_PALETTE_SIZE;
    bmfh.bfSize = SWAP32cnd(bmfh.bfSize, isMBE);
    bmfh.bfReserved1 = 0;
    bmfh.bfReserved2 = 0;
    bmfh.bfOffBits = sizeof(bmih) + sizeof(bmfh);
    if (pixFmt == PixelFormat::PIX_PAL8 || pixFmt == PixelFormat::PIX_MONO8)
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

    file.write(reinterpret_cast<const char*>(&bmfh), sizeof(bmfh));
    file.write(reinterpret_cast<const char*>(&bmih), sizeof(bmih));
    if (pixFmt == PixelFormat::PIX_PAL8 || pixFmt == PixelFormat::PIX_MONO8)
    {
        uint8_t palette[BMP_PALETTE_SIZE]; // BMP-style palette

        if (pixFmt == PixelFormat::PIX_PAL8)
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

        file.write(reinterpret_cast<const char*>(palette), BMP_PALETTE_SIZE);
    }

    int skip = bmpLineWidth - img.GetWidth() * bytesPP;

    std::unique_ptr<std::uint8_t[]> row;
    if (pixFmt == PixelFormat::PIX_RGB8)
    {
        row = std::make_unique<uint8_t[]>(img.GetWidth() * bytesPP);
    }

    for (i = img.GetHeight() - 1; i >= 0; i--) // lines in a BMP are stored bottom to top
    {
        if (pixFmt == PixelFormat::PIX_RGB8)
        {
            const auto* srcRow = img.GetRowAs<std::uint8_t>(i);
            for (unsigned x = 0; x < img.GetWidth(); x++)
            {
                row[3*x + 0] = srcRow[3*x + 2];
                row[3*x + 1] = srcRow[3*x + 1];
                row[3*x + 2] = srcRow[3*x + 0];
            }
            file.write(reinterpret_cast<const char*>(row.get()), img.GetWidth() * bytesPP);
        }
        else
            file.write(img.GetRowAs<char>(i), img.GetWidth() * bytesPP);

        if (skip > 0)
            file.write(img.GetRowAs<char>(i), skip); //this is just padding, so write anything
    }

    file.close();

    return true;
}

std::optional<std::tuple<unsigned, unsigned>> GetBmpDimensions(const std::filesystem::path& fileName)
{
    std::ifstream file(fileName, std::ios_base::in | std::ios_base::binary);
    if (file.fail())
        return std::nullopt;

    BITMAPFILEHEADER_t bmpFileHdr;
    BITMAPINFOHEADER_t bmpInfoHdr;

    file.read(reinterpret_cast<char*>(&bmpFileHdr), sizeof(bmpFileHdr));
    file.read(reinterpret_cast<char*>(&bmpInfoHdr), sizeof(bmpInfoHdr));
    if (file.eof())
        return std::nullopt;

    // Fields in a BMP are always little-endian, so swap them if running on a big-endian machine
    const bool isMBE = IsMachineBigEndian();

    return std::make_tuple(
        SWAP32cnd(bmpInfoHdr.biWidth, isMBE),
        SWAP32cnd(bmpInfoHdr.biHeight, isMBE)
    );
}
