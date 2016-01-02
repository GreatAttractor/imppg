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
    TIFF-related functions.
*/

#include <limits.h>
#include <cassert>
#include <fstream>
#include <boost/scoped_array.hpp>
#include <boost/format.hpp>
#include "tiff.h"

using boost::scoped_array;

#pragma pack(push, 1)
typedef struct
{
    uint16_t tag;
    uint16_t type;
    uint32_t count;
    uint32_t value;
} TiffField_t;

typedef struct
{
    uint16_t id;
    uint16_t version;
    uint32_t dirOffset; // = offset of 'numDirEntries'
} TiffHeader_t;

#pragma pack(pop)

typedef enum { ttByte = 1, ttAscii, ttWord, ttDWord, ttRational } TagType_t;

const int TIFF_VERSION = 42;
const int TAG_IMAGE_WIDTH =                0x100;
const int TAG_IMAGE_HEIGHT =               0x101;
const int TAG_BITS_PER_SAMPLE =            0x102;
const int TAG_COMPRESSION =                0x103;
const int TAG_PHOTOMETRIC_INTERPRETATION = 0x106;
const int TAG_STRIP_OFFSETS =              0x111;
const int TAG_SAMPLES_PER_PIXEL =          0x115;
const int TAG_ROWS_PER_STRIP =             0x116;
const int TAG_STRIP_BYTE_COUNTS =          0x117;
const int TAG_PLANAR_CONFIGURATION =       0x11C;

const uint16_t NO_COMPRESSION = 1;
const uint16_t PLANAR_CONFIGURATION_CHUNKY = 1;
const uint16_t INTEL_BYTE_ORDER = ((uint16_t)'I' << 8) + 'I'; // little-endian
const uint16_t MOTOROLA_BYTE_ORDER = ((uint16_t)'M' << 8) + 'M'; // big-endian
const int PHMET_WHITE_IS_ZERO = 0;
const int PHMET_BLACK_IS_ZERO = 1;
const int PHMET_RGB = 2;

bool IsMachineBigEndian();

inline unsigned GetFieldTypeLength(TagType_t ttt)
{
    switch (ttt)
    {
    case ttByte: return 1; break;
    case ttAscii: return 1; break;
    case ttWord: return 2; break;
    case ttDWord: return 4; break;
    case ttRational: return 8; break;
    }
}

/// Changes endianess of 16-bit words in the specified buffer
void SwapBufferWords(IImageBuffer &buf)
{
    for (int j = 0; j < buf.GetHeight(); j++)
    {
        uint16_t *row = (uint16_t *)buf.GetRow(j);
        for (int i = 0; i < buf.GetWidth(); i++)
            row[i] = (row[i] << 8) | (row[i] >> 8);
    }
}

/// Reverses values of an 8-bit grayscale buffer
void NegateGrayscale8(IImageBuffer &buf)
{
    for (int j = 0; j < buf.GetHeight(); j++)
    {
        uint8_t *row = (uint8_t *)buf.GetRow(j);
        for (int i = 0; i < buf.GetWidth(); i++)
            row[i] = 0xFF - row[i];
    }
}

/// Reverses values of a 16-bit grayscale buffer
void NegateGrayscale16(IImageBuffer &buf)
{
    for (int j = 0; j < buf.GetHeight(); j++)
    {
        uint16_t *row = (uint16_t *)buf.GetRow(j);
        for (int i = 0; i < buf.GetWidth(); i++)
            row[i] = 0xFFFF - row[i];
    }
}

bool GetTiffDimensions(const char *fileName, unsigned &imgWidth, unsigned &imgHeight)
{
    std::ifstream file(fileName, std::ios_base::binary);

    if (file.fail())
        return false;

    TiffHeader_t tiffHeader;
    file.read((char *)&tiffHeader, sizeof(tiffHeader));
    if (file.gcount() != sizeof(tiffHeader))
        return false;

    bool isMBE = IsMachineBigEndian();
    bool isFBE = tiffHeader.id == MOTOROLA_BYTE_ORDER; // true if the file has big endian data
    bool enDiff = (isMBE != isFBE);

    if (SWAP16cnd(tiffHeader.version, enDiff) != TIFF_VERSION)
        return false;

    // Seek to the first TIFF directory
    file.seekg(SWAP32cnd(tiffHeader.dirOffset, enDiff), std::ios_base::beg);

    uint16_t numDirEntries;
    file.read((char *)&numDirEntries, sizeof(numDirEntries));
    numDirEntries = SWAP16cnd(numDirEntries, enDiff);
    if (file.gcount() != sizeof(numDirEntries))
        return false;

    imgWidth = imgHeight = UINT_MAX;

    std::fstream::pos_type nextFieldPos = file.tellg();
    for (unsigned i = 0; i < numDirEntries; i++)
    {
        TiffField_t tiffField;

        file.seekg(nextFieldPos, std::ios_base::beg);
        file.read((char *)&tiffField, sizeof(tiffField));
        if (file.gcount() != sizeof(tiffField))
            return false;
        nextFieldPos = file.tellg();

        tiffField.tag = SWAP16cnd(tiffField.tag, enDiff);
        tiffField.type = SWAP16cnd(tiffField.type, enDiff);
        tiffField.count = SWAP32cnd(tiffField.count, enDiff);
        if (tiffField.count > 1 || tiffField.type == ttDWord)
            tiffField.value = SWAP32cnd(tiffField.value, enDiff);
        else if (tiffField.count == 1 && tiffField.type == ttWord)
        {
            // This is a special case where a 16-bit value is stored in a 32-bit field,
            // always in the lower-address bytes. So if the machine is big-endian,
            // the value always has to be shifted right by 16 bits first, regardless of
            // the file's endianess, and only then swapped, if the machine and file endianesses differ.
            if (isMBE)
                tiffField.value >>= 16;

            tiffField.value = SWAP16in32cnd(tiffField.value, enDiff);
        }

        switch (tiffField.tag)
        {
        case TAG_IMAGE_WIDTH: imgWidth = tiffField.value; break;
        case TAG_IMAGE_HEIGHT: imgHeight = tiffField.value; break;
        }

        if (imgWidth != UINT_MAX && imgHeight != UINT_MAX)
            break;
    }

    return true;
}

/// Saves image in TIFF format; returns 'false' on error
bool SaveTiff(const char *fileName, c_Image &img)
{
    assert(img.GetPixelFormat() == PIX_MONO8 ||
           img.GetPixelFormat() == PIX_MONO16 ||
           img.GetPixelFormat() == PIX_RGB8 ||
           img.GetPixelFormat() == PIX_RGB16);

    std::ofstream file(fileName, std::ios_base::trunc | std::ios_base::binary);

    if (file.fail())
        return false;

    bool isMBE = IsMachineBigEndian();

    // Note: a 16-bit value (a "ttWord") stored in the 32-bit TiffField_t::value field has to be
    // always "left-aligned", i.e. stored in the lower-address two bytes in the file, regardless
    // of the file's and machine's endianess.
    //
    // This means that on a big-endian machine it has to be always shifted left by 16 bits
    // prior to writing to the file.

    TiffHeader_t tiffHeader;
    tiffHeader.id = isMBE ? MOTOROLA_BYTE_ORDER : INTEL_BYTE_ORDER;
    tiffHeader.version = TIFF_VERSION;
    tiffHeader.dirOffset = sizeof(tiffHeader);
    file.write((const char *)&tiffHeader, sizeof(tiffHeader));

    uint16_t numDirEntries = 10;
    file.write((const char *)&numDirEntries, sizeof(numDirEntries));

    uint32_t nextDirOffset = 0;

    TiffField_t field;

    field.tag = TAG_IMAGE_WIDTH;
    field.type = ttWord;
    field.count = 1;
    field.value = img.GetWidth();
    if (isMBE) field.value <<= 16;
    file.write((const char *)&field, sizeof(field));

    field.tag = TAG_IMAGE_HEIGHT;
    field.type = ttWord;
    field.count = 1;
    field.value = img.GetHeight();
    if (isMBE) field.value <<= 16;
    file.write((const char *)&field, sizeof(field));

    field.tag = TAG_BITS_PER_SAMPLE;
    field.type = ttWord;
    field.count = 1;
    switch (img.GetPixelFormat())
    {
    case PIX_MONO8:
    case PIX_RGB8:
        field.value = 8; break;
    case PIX_MONO16:
    case PIX_RGB16:
        field.value = 16; break;
    }
    if (isMBE) field.value <<= 16;
    file.write((const char *)&field, sizeof(field));

    field.tag = TAG_COMPRESSION;
    field.type = ttWord;
    field.count = 1;
    field.value = NO_COMPRESSION;
    if (isMBE) field.value <<= 16;
    file.write((const char *)&field, sizeof(field));

    field.tag = TAG_PHOTOMETRIC_INTERPRETATION;
    field.type = ttWord;
    field.count = 1;
    switch (img.GetPixelFormat())
    {
    case PIX_MONO8:
    case PIX_MONO16:
        field.value = PHMET_BLACK_IS_ZERO; break;
    case PIX_RGB8:
    case PIX_RGB16:
        field.value = PHMET_RGB; break;
    }
    if (isMBE) field.value <<= 16;
    file.write((const char *)&field, sizeof(field));

    field.tag = TAG_STRIP_OFFSETS;
    field.type = ttDWord;
    field.count = 1;
    // we write the header, num. of directory entries, 10 fields and a next directory offset (==0); pixel data starts next
    field.value = sizeof(tiffHeader) + sizeof(numDirEntries) + 10*sizeof(field) + sizeof(nextDirOffset);
    file.write((const char *)&field, sizeof(field));

    field.tag = TAG_SAMPLES_PER_PIXEL;
    field.type = ttWord;
    field.count = 1;
    switch (img.GetPixelFormat())
    {
    case PIX_MONO8:
    case PIX_MONO16:
        field.value = 1; break;
    case PIX_RGB8:
    case PIX_RGB16:
        field.value = 3; break;
    }
    if (isMBE) field.value <<= 16;
    file.write((const char *)&field, sizeof(field));

    field.tag = TAG_ROWS_PER_STRIP;
    field.type = ttWord;
    field.count = 1;
    field.value = img.GetHeight(); // there is only one strip for the whole image
    if (isMBE) field.value <<= 16;
    file.write((const char *)&field, sizeof(field));

    field.tag = TAG_STRIP_BYTE_COUNTS;
    field.type = ttDWord;
    field.count = 1;
    field.value = img.GetWidth() * img.GetHeight() * BytesPerPixel[img.GetPixelFormat()]; // there is only one strip for the whole image
    file.write((const char *)&field, sizeof(field));

    field.tag = TAG_PLANAR_CONFIGURATION;
    field.type = ttWord;
    field.count = 1;
    field.value = PLANAR_CONFIGURATION_CHUNKY; // there is only one strip for the whole image
    if (isMBE) field.value <<= 16;
    file.write((const char *)&field, sizeof(field));

    // write the next directory offset (0 = no other directories)
    file.write((const char *)&nextDirOffset, sizeof(nextDirOffset));

    for (unsigned i = 0; i < img.GetHeight(); i++)
        file.write((const char *)img.GetRow(i), img.GetWidth() * BytesPerPixel[img.GetPixelFormat()]);

    file.close();
    return true;
}

/// Reads a TIFF image; returns 0 on error
c_Image *ReadTiff(
    const char *fileName,
    std::string *errorMsg ///< If not null, receives error message (if any));
)
{
    int imgWidth = -1, imgHeight = -1;
    PixelFormat_t pixFmt;

    std::ifstream file(fileName, std::ios_base::binary);

    if (file.fail())
        return 0;

    TiffHeader_t tiffHeader;
    file.read((char *)&tiffHeader, sizeof(tiffHeader));
    if (file.gcount() != sizeof(tiffHeader))
    {
        if (errorMsg) *errorMsg = "File header is incomplete.";
        return 0;
    }

    bool isMBE = IsMachineBigEndian();
    bool isFBE = tiffHeader.id == MOTOROLA_BYTE_ORDER; // true if the file has big endian data
    bool enDiff = (isMBE != isFBE);

    if (SWAP16cnd(tiffHeader.version, enDiff) != TIFF_VERSION)
    {
        if (errorMsg) *errorMsg = "Unknown TIFF version.";
        return 0;
    }

    // Seek to the first TIFF directory
    file.seekg(SWAP32cnd(tiffHeader.dirOffset, enDiff), std::ios_base::beg);

    uint16_t numDirEntries;
    file.read((char *)&numDirEntries, sizeof(numDirEntries));
    numDirEntries = SWAP16cnd(numDirEntries, enDiff);
    if (file.gcount() != sizeof(numDirEntries))
    {
        if (errorMsg) *errorMsg = "The number of TIFF directory entries tag is incomplete.";
        return 0;
    }

    unsigned numStrips = 0;
    unsigned bitsPerSample = 0;
    scoped_array<uint32_t> stripOffsets;
    scoped_array<uint32_t> stripByteCounts;
    unsigned rowsPerStrip = 0;
    int photometricInterpretation = -1;
    int samplesPerPixel = 0;

    std::fstream::pos_type nextFieldPos = file.tellg();
    for (unsigned i = 0; i < numDirEntries; i++)
    {
        TiffField_t tiffField;

        file.seekg(nextFieldPos, std::ios_base::beg);
        file.read((char *)&tiffField, sizeof(tiffField));
        if (file.gcount() != sizeof(tiffField))
        {
            if (errorMsg) *errorMsg = "TIFF field is incomplete.";
            return 0;
        }
        nextFieldPos = file.tellg();

        tiffField.tag = SWAP16cnd(tiffField.tag, enDiff);
        tiffField.type = SWAP16cnd(tiffField.type, enDiff);
        tiffField.count = SWAP32cnd(tiffField.count, enDiff);
        if (tiffField.count > 1 || tiffField.type == ttDWord)
            tiffField.value = SWAP32cnd(tiffField.value, enDiff);
        else if (tiffField.count == 1 && tiffField.type == ttWord)
        {
            // This is a special case where a 16-bit value is stored in a 32-bit field,
            // always in the lower-address bytes. So if the machine is big-endian,
            // the value always has to be shifted right by 16 bits first, regardless of
            // the file's endianess, and only then swapped, if the machine and file endianesses differ.
            if (isMBE)
                tiffField.value >>= 16;

            tiffField.value = SWAP16in32cnd(tiffField.value, enDiff);
        }

        switch (tiffField.tag)
        {
        case TAG_IMAGE_WIDTH: imgWidth = tiffField.value; break;

        case TAG_IMAGE_HEIGHT: imgHeight = tiffField.value; break;

        case TAG_BITS_PER_SAMPLE:
            if (tiffField.count == 1)
                bitsPerSample = tiffField.value;
            else
            {
                // Some files may have as many "bits per sample" values specified
                // as there are channels. Make sure they are all the same.

                file.seekg(tiffField.value, std::ios_base::beg);

                scoped_array<uint16_t> fieldBuf(new uint16_t[tiffField.count]);
                file.read((char *)fieldBuf.get(), tiffField.count * sizeof(uint16_t));

                bool allEqual = true;
                uint16_t first = fieldBuf[0];
                for (unsigned j = 1; j < tiffField.count; j++)
                    if (fieldBuf[j] != first)
                    {
                        allEqual = false;
                        break;
                    }

                 if (!allEqual)
                 {
                    if (errorMsg) *errorMsg = "Files with differing bit depts per channel are not supported.";
                    return 0;
                 }

                 bitsPerSample = SWAP16cnd(first, enDiff);
            }

            if (bitsPerSample != 8 && bitsPerSample != 16)
            {
                if (errorMsg) *errorMsg = "Only 8 and 16 bits per channel files are supported.";
                return 0;
            }
            break;

        case TAG_COMPRESSION:
            if (tiffField.value != NO_COMPRESSION)
            {
                if (errorMsg) *errorMsg = "Compression is not supported.";
                return 0;
            }
            break;

        case TAG_PHOTOMETRIC_INTERPRETATION: photometricInterpretation = tiffField.value; break;

        case TAG_STRIP_OFFSETS:
            numStrips = tiffField.count;
            stripOffsets.reset(new uint32_t[numStrips]);
            if (numStrips == 1)
                stripOffsets[0] = tiffField.value;
            else
            {
                file.seekg(tiffField.value, std::ios_base::beg);
                for (unsigned i = 0; i < numStrips; i++)
                {
                    file.read((char *)&stripOffsets[i], sizeof(stripOffsets[i]));
                    stripOffsets[i] = SWAP32cnd(stripOffsets[i], enDiff);
                }
            }
            break;

        case TAG_SAMPLES_PER_PIXEL: samplesPerPixel = tiffField.value; break;

        case TAG_ROWS_PER_STRIP: rowsPerStrip = tiffField.value; break;

        case TAG_STRIP_BYTE_COUNTS:
            stripByteCounts.reset(new uint32_t[tiffField.count]);
            if (tiffField.count == 1)
                stripByteCounts[0] = tiffField.value;
            else
            {
                file.seekg(tiffField.value, std::ios_base::beg);
                for (unsigned i = 0; i < tiffField.count; i++)
                {
                    file.read((char *)&stripByteCounts[i], sizeof(stripByteCounts[i]));
                    stripByteCounts[i] = SWAP32cnd(stripByteCounts[i], enDiff);
                }
            }
            break;

        case TAG_PLANAR_CONFIGURATION:
            if (tiffField.value != PLANAR_CONFIGURATION_CHUNKY)
            {
                if (errorMsg) *errorMsg = "Files with planar configuration other than packed (chunky) are not supported.";
                return 0;
            }
            break;
        }
    }

    if (rowsPerStrip == 0 && numStrips == 1)
        // If there is only 1 strip, it contains all the rows
        rowsPerStrip = imgHeight;

    // Validate the values

    if (samplesPerPixel == 1 && photometricInterpretation != PHMET_BLACK_IS_ZERO && photometricInterpretation != PHMET_WHITE_IS_ZERO ||
        samplesPerPixel == 3 && photometricInterpretation != PHMET_RGB ||
        samplesPerPixel != 1 && samplesPerPixel != 3)
    {
        if (errorMsg) *errorMsg = "Only RGB and grayscale images are supported.";
        return 0;
    }

    if (samplesPerPixel == 1)
    {
        if (bitsPerSample == 8)
            pixFmt = PIX_MONO8;
        else if (bitsPerSample == 16)
            pixFmt = PIX_MONO16;
    }
    else if (samplesPerPixel == 3)
    {
        if (bitsPerSample == 8)
            pixFmt = PIX_RGB8;
        else if (bitsPerSample == 16)
            pixFmt = PIX_RGB16;
    }

    c_Image *result = new c_Image(imgWidth, imgHeight, pixFmt);

    //int bufOfs = 0;
    int currentRow = 0;
    for (unsigned i = 0; i < numStrips; i++)
    {
        file.seekg(stripOffsets[i], std::ios_base::beg);
        
        for (unsigned j = 0; j < rowsPerStrip && currentRow < imgHeight; j++, currentRow++)
        {
            std::streamsize numBytesToRead = imgWidth * BytesPerPixel[pixFmt];
            file.read((char *)result->GetRow(currentRow), numBytesToRead);

            if (file.gcount() != numBytesToRead)
            {
                if (errorMsg) *errorMsg = boost::str(boost::format("The file is incomplete: pixel data in strip %d is too short. "
                    "Expected %d bytes, but read only %d.")
                    % i
                    % stripByteCounts[i]
                    % (currentRow * numBytesToRead));
                delete result;
                return 0;
            }
        }
    }

    if ((pixFmt == PIX_MONO16 || pixFmt == PIX_RGB16) && enDiff)
        SwapBufferWords(result->GetBuffer());

    if (photometricInterpretation == PHMET_WHITE_IS_ZERO)
    {
        // Reverse the values so that "black" is zero, "white" is 255 or 65535.
        if (pixFmt == PIX_MONO8)
            NegateGrayscale8(result->GetBuffer());
        else if (pixFmt == PIX_MONO16)
            NegateGrayscale16(result->GetBuffer());
    }

    file.close();

    return result;
}
