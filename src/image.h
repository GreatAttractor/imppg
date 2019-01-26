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
    Image-handling code header.
*/

#ifndef ImPPG_IMAGE_H
#define ImPPG_IMAGE_H

#include <cstdint>
#include <memory>
#include <string>
#if USE_FREEIMAGE
#include "FreeImage.h"
#endif
#include "formats.h"


/// Conditionally swaps a 32-bit value
uint32_t SWAP32cnd(uint32_t x, bool swap);
/// Conditionally swaps the two lower (arithmetically) bytes of a 32-bit value
uint32_t SWAP16in32cnd(uint32_t x, bool swap);
/// Conditionally swaps a 16-bit value
uint16_t SWAP16cnd(uint16_t x, bool swap);

/// Image pixel format
typedef enum {
    PIX_INVALID = 0, // this has to be the first element

    PIX_UNCHANGED, ///< Use the source pixel format
    PIX_PAL8,      ///< 8-bit with palette (can be a greyscale palette)
    PIX_MONO8,     ///< 8-bit greyscale
    PIX_RGB8,      ///< 24-bit RGB (8 bits per channel)
    PIX_RGBA8,     ///< 32-bit RGBA (8 bits per channel)
    PIX_MONO16,    ///< 16-bit greyscale
    PIX_RGB16,     ///< 48-bit RGB (16 bits per channel)
    PIX_RGBA16,    ///< 64-bit RGBA (16 bits per channel)
    PIX_MONO32F,   ///< 32-bit floating point greyscale
    PIX_RGB32F,    ///< 96-bit floating point RGB
    PIX_RGBA32F,   ///< 128-bit floating point RGBA

    PIX_NUM_FORMATS // this has to be the last element
} PixelFormat_t;

/// Elements correspond with those from PixelFormat_t
const size_t BytesPerPixel[PIX_NUM_FORMATS] =
{
    0, 0,
    1,    // PIX_PAL8
    1,    // PIX_MONO8
    3,    // PIX_RBG8
    4,    // PIX_RBGA8
    2,    // PIX_MONO16
    6,    // PIX_RGB16
    8,    // PIX_RGBA16
    4,    // PIX_MONO32F
    12,   // PIX_RGB32F
    16,   // PIX_RGBA32F
};

/// Elements correspond with those from PixelFormat_t
const size_t NumChannels[PIX_NUM_FORMATS] =
{
    0, 0,
    1,    // PIX_PAL8
    1,    // PIX_MONO8
    3,    // PIX_RBG8
    4,    // PIX_RBGA8
    1,    // PIX_MONO16
    3,    // PIX_RGB16
    4,    // PIX_RGBA16
    1,    // PIX_MONO32F
    3,    // PIX_RGB32F
    4,    // PIX_RGBA32F
};

/// Image buffer interface
class IImageBuffer
{
public:
    virtual unsigned GetWidth() const = 0; ///< Returns image width in pixels
    virtual unsigned GetHeight() const = 0; ///< Returns image height in pixels
    virtual size_t GetBytesPerRow() const = 0; ///< Returns number of bytes per row (including padding, if any)
    virtual size_t GetBytesPerPixel() const = 0; ///< Returns number of bytes per pixel
    virtual void *GetRow(size_t row) = 0; ///< Returns pointer to the specified row
    virtual const void *GetRow(size_t row) const = 0; ///< Returns pointer to the specified row
    virtual std::unique_ptr<IImageBuffer> GetCopy() const = 0; ///< Creates and returns a copy
    virtual PixelFormat_t GetPixelFormat() const = 0;
    virtual ~IImageBuffer() { }
};

class c_Image
{
private:
    std::unique_ptr<IImageBuffer> pixels;

    /// Palette contents (R, G, B) if pixFmt == PIX_PAL8; contains PALETTE_LENGTH elements
    std::unique_ptr<uint8_t[]> palette;

public:
    const size_t PALETTE_LENGTH = 256 * 3;

    /// Creates an uninitialized image (0x0 pixels, PIX_INVALID pixel format); IsValid() will return false
    c_Image();

    c_Image(unsigned width, unsigned height, PixelFormat_t pixFmt);

    c_Image(std::unique_ptr<IImageBuffer> buffer);

    c_Image(c_Image &&img);

    c_Image &operator=(const c_Image &img);

    c_Image &operator=(c_Image &&img);

    c_Image(const c_Image &img);

    /// Returns an uninitialized image (0x0 pixels, PIX_INVALID pixel format); IsValid() will return false
    static c_Image GetEmpty() { return c_Image(); }

    bool IsValid() { return pixels->GetPixelFormat() != PIX_INVALID; }

    void ClearToZero(); ///< Clears all pixels to zero value

    /// Provides read/write access to the palette
    uint8_t *GetPalette() { return palette.get(); }

    /// Provides read-only access to the palette
    const uint8_t *GetPalette() const { return palette.get(); }

    unsigned GetWidth() const { return pixels->GetWidth(); }
    unsigned GetHeight() const { return pixels->GetHeight(); }
    unsigned GetNumPixels() const { return GetWidth() * GetHeight(); }
    PixelFormat_t GetPixelFormat() const { return pixels->GetPixelFormat(); }

    /// Returns pointer to the specified row
    void *GetRow(size_t row) { return pixels->GetRow(row); }

    /// Returns pointer to the specified row
    const void *GetRow(size_t row) const { return pixels->GetRow(row); }

    IImageBuffer &GetBuffer() { return *pixels; }

    const IImageBuffer &GetBuffer() const { return *pixels; }

    void SetPalette(uint8_t palette[]);

    /// Copies a rectangular area from 'src' to 'dest'. Pixel formats of 'src' and 'dest' have to be the same.
    static void Copy(const c_Image &src, c_Image &dest, unsigned srcX, unsigned srcY, unsigned width, unsigned height, unsigned destX, unsigned destY);

    /** Converts 'srcImage' to 'destPixFmt'; ; result uses c_SimpleBuffer for storage.
        If 'destPixFmt' is the same as source image's pixel format, returns a copy of 'srcImg'. */
    static c_Image ConvertPixelFormat(const c_Image &srcImg, PixelFormat_t destPixFmt);

    /// Converts the specified fragment of 'srcImage' to 'destPixFmt'; result uses c_SimpleBuffer for storage
    static c_Image ConvertPixelFormat(const c_Image &srcImg, PixelFormat_t destPixFmt, unsigned x0, unsigned y0, unsigned width, unsigned height);

    /// Resizes and translates image (or its fragment) by cropping and/or padding (with zeros) to the destination size and offset (there is no scaling)
    /** Subpixel translation (i.e. if 'xOfs' or 'yOfs' have a fractional part) of palettised images is not supported. */
    static void ResizeAndTranslate(
        IImageBuffer &srcImg,
        IImageBuffer &destImg,
        int srcXmin,     ///< X min of input data in source image
        int srcYmin,     ///< Y min of input data in source image
        int srcXmax,     ///< X max of input data in source image
        int srcYmax,     ///< Y max of input data in source image
        float xOfs,      ///< X offset of input data in output image
        float yOfs,      ///< Y offset of input data in output image
        bool clearToZero ///< If 'true', 'destImg' areas not copied on will be cleared to zero
        );

    /// Multiply by another image; both images have to be PIX_MONO32F and have the same dimensions
    void Multiply(const c_Image &mult);
};

/// Lightweight wrapper of a fragment of an image buffer; does not allocate any pixels memory itself
class c_ImageBufferView : public IImageBuffer
{
    IImageBuffer &buf; ///< The underlying image buffer
    int x0, y0, width, height;
public:
    c_ImageBufferView(c_Image &img)
        : buf(img.GetBuffer()), x0(0), y0(0), width(img.GetWidth()), height(img.GetHeight())
    { }

    c_ImageBufferView(IImageBuffer &underlyingBuffer, int x0, int y0, int width, int height)
        : buf(underlyingBuffer), x0(x0), y0(y0), width(width), height(height)
    { }

    /// Returns image width in pixels
    virtual unsigned GetWidth() const { return width; }

    /// Returns image height in pixels
    virtual unsigned GetHeight() const { return height; }

    /// Returns number of bytes per row (including padding, if any)
    virtual size_t GetBytesPerRow() const { return buf.GetBytesPerRow(); }

    /// Returns number of bytes per pixel
    virtual size_t GetBytesPerPixel() const { return buf.GetBytesPerPixel(); }

    /// Returns pointer to the specified row
    virtual void *GetRow(size_t row) { return (uint8_t *)buf.GetRow(y0 + row) + x0 * buf.GetBytesPerPixel(); }

    /// Returns pointer to the specified row
    virtual const void *GetRow(size_t row) const { return (const uint8_t *)buf.GetRow(y0 + row) + x0 * buf.GetBytesPerPixel(); }

    virtual PixelFormat_t GetPixelFormat() const { return buf.GetPixelFormat(); }

    /// Creating a copy of a view is not supported
    virtual std::unique_ptr<IImageBuffer> GetCopy() const { return nullptr; }
};

#if USE_FREEIMAGE
/// A wrapper around a FreeImage bitmap object; does not allocate nor free any memory itself
/** Palettised bitmaps are not supported. Before using c_FreeImageBuffer, convert the wrapped FI bitmap to RGB8. */
class c_FreeImageBuffer: public IImageBuffer
{
    FIBITMAP *fibmp;
    uint8_t *pixels;
    int stride;

public:

    c_FreeImageBuffer(FIBITMAP *fibmp): fibmp(fibmp)
    {
        // Store the pointer to the beginning of pixel data and the stride
        // and later calculate row pointers in GetRow(), because for some reason
        // using FreeImage_GetScanLine() is very slow (as of FreeImage 3.16.0,
        // on Windows 8 x64 and Fedora 21 x64).
        pixels = FreeImage_GetBits(fibmp);
        stride = FreeImage_GetPitch(fibmp);
    }

    /// Returns image width in pixels
    virtual unsigned GetWidth() const { return FreeImage_GetWidth(fibmp); }

    /// Returns image height in pixels
    virtual unsigned GetHeight() const { return FreeImage_GetHeight(fibmp); }

    /// Returns number of bytes per row (including padding, if any)
    virtual size_t GetBytesPerRow() const { return FreeImage_GetLine(fibmp); }

    /// Returns number of bytes per pixel
    virtual size_t GetBytesPerPixel() const { return FreeImage_GetBPP(fibmp) / 8; }

    /// Returns pointer to the specified row; FreeImage stores rows in reverse order
    virtual void *GetRow(size_t row) { return pixels + (GetHeight() - 1 - row) * stride; }

    /// Returns pointer to the specified row; FreeImage stores rows in reverse order
    virtual const void *GetRow(size_t row) const { return pixels + (GetHeight() - 1 - row) * stride; }

    virtual PixelFormat_t GetPixelFormat() const
    {
        switch (FreeImage_GetImageType(fibmp))
        {
        case FIT_BITMAP:
            switch (GetBytesPerPixel())
            {
            case 3: return PIX_RGB8;
            case 4: return PIX_RGBA8;
            default: return PIX_INVALID;
            }
            break;

        case FIT_UINT16: return PIX_MONO16;
        case FIT_FLOAT:  return PIX_MONO32F;
        case FIT_RGB16:  return PIX_RGB16;
        case FIT_RGBA16: return PIX_RGBA16;
        case FIT_RGBF:   return PIX_RGB32F;
        case FIT_RGBAF:  return PIX_RGBA32F;

        default: return PIX_INVALID;
        }
    }

    /// Creating a copy is not supported
    virtual std::unique_ptr<IImageBuffer> GetCopy() const { return nullptr; }
};
#endif // USE_FREEIMAGE

void NormalizeFpImage(c_Image &img, float minLevel, float maxLevel);

/// Loads the specified image file and converts it to PIX_MONO32F; returns 0 on error
c_Image LoadImageFileAsMono32f(
    const std::string &fname,        ///< Full path (including file name and extension)
    const std::string &extension,    ///< lowercase extension
    std::string *errorMsg = 0        ///< If not null, may receive an error message (if any)
);

/// Loads the specified image file and converts it to PIX_MONO8; returns 0 on error
c_Image LoadImageFileAsMono8(
    const std::string &fname,     ///< Full path (including file name and extension)
    const std::string &extension, ///< lowercase extension
    std::string *errorMsg = 0     ///< If not null, may receive an error message (if any)
);

#if USE_CFITSIO
/// Loads an image from a FITS file; the result's pixel format will be PIX_MONO8, PIX_MONO16 or PIX_MONO32F
c_Image LoadFitsImage(const std::string &fname);
#endif

/// Saves image using the specified output format
bool SaveImageFile(
    const std::string &fname, ///< Full destination path
    const c_Image &img,
    OutputFormat outputFmt
);

/// Returns 'true' if image's width and/or height were successfully read; returns 'false' on error
bool GetImageSize(
    const std::string &fname,     ///< Full path (including file name and extension)
    const std::string &extension, ///< lowercase extension
    unsigned &width,              ///< Receives image width
    unsigned &height              ///< Receives image height
);

#endif // ImPPG_HEADER_H
