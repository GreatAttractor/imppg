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
    Image-handling code.
*/

#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <cfloat>
#include <cassert>
#include <algorithm>
#include <boost/format.hpp>

#include "image.h"
#if (USE_FREEIMAGE)
  #include "FreeImage.h"
#else
  #include "bmp.h"
  #include "tiff.h"
#endif

#if USE_CFITSIO
  #if defined(_WIN32)
    #include <fitsio.h>
  #else
    #include <cfitsio/fitsio.h>
  #endif
#endif

/// Conditionally swaps a 32-bit value
uint32_t SWAP32cnd(uint32_t x, bool swap)
{
    if (swap) return (x << 24) | ((x & 0x00FF0000) >> 8) | ((x & 0x0000FF00) << 8) | (x >> 24);
    else return x;
}

/// Conditionally swaps two lower bytes of a 32-bit value
uint32_t SWAP16in32cnd(uint32_t x, bool swap)
{
    if (swap) return ((x & 0xFF) << 8) | (x >> 8);
    else return x;
}

uint16_t SWAP16cnd(uint16_t x, bool swap)
{
    if (swap) return (x << 8) | (x >> 8);
    else return x;
}

bool IsMachineBigEndian()
{
    static uint16_t value16 = 0x1122;
    static uint8_t *ptr = (uint8_t *)&value16;
    return (ptr[0] == 0x11);
}

/// Simple image buffer; pixels are stored in row-major order with no padding.
class c_SimpleBuffer: public IImageBuffer
{
    PixelFormat_t pixFmt;
    unsigned width, height;
    size_t bytesPerPixel;
    void *buffer;

public:
    c_SimpleBuffer(int width, int height, PixelFormat_t pixFmt):
        pixFmt(pixFmt), width(width), height(height), bytesPerPixel(BytesPerPixel[pixFmt])
    {
        buffer = operator new[](width * height * bytesPerPixel);
    }

    virtual unsigned GetWidth() const { return width; }
    virtual unsigned GetHeight() const { return height; }
    virtual size_t GetBytesPerRow() const { return width * bytesPerPixel; }
    virtual size_t GetBytesPerPixel() const { return bytesPerPixel; }
    virtual void *GetRow(size_t row) { return (uint8_t *)buffer + row * width * bytesPerPixel; }
    virtual const void *GetRow(size_t row) const { return (uint8_t *)buffer + row * width * bytesPerPixel; }
    virtual PixelFormat_t GetPixelFormat() const { return pixFmt; }
    virtual std::unique_ptr<IImageBuffer> GetCopy() const
    {
        std::unique_ptr<c_SimpleBuffer> copy(new c_SimpleBuffer(width, height, pixFmt));
        memcpy(copy->buffer, buffer, width * height * bytesPerPixel);
        return std::move(copy);
    }

    virtual ~c_SimpleBuffer()
    {
        operator delete[](buffer);
    }
};

c_Image::c_Image(): width(0), height(0)
{
    palette.reset(new uint8_t[PALETTE_LENGTH]);
}

/// Allocates memory for pixel data
c_Image::c_Image(unsigned width, unsigned height, PixelFormat_t pixFmt): c_Image()
{
    this->width = width;
    this->height = height;
    assert(pixFmt > PIX_UNCHANGED && pixFmt < PIX_NUM_FORMATS);
    assert(width > 0 && height > 0);

    pixels.reset(new c_SimpleBuffer(width, height, pixFmt));

    memset(palette.get(), 0, sizeof(palette));
}

c_Image::c_Image(std::unique_ptr<IImageBuffer> buffer): c_Image()
{
    pixels = std::move(buffer);
    width = pixels->GetWidth();
    height = pixels->GetHeight();
}

/// Performs a deep copy of 'img'
c_Image::c_Image(const c_Image &img): c_Image()
{
    width = img.width;
    height = img.height;
    memcpy(palette.get(), img.palette.get(), PALETTE_LENGTH);
    pixels = img.pixels->GetCopy();
}

/// Clears all pixels to zero value
void c_Image::ClearToZero()
{
    // Works also for an array of floats; 32 zero bits represent a floating-point 0.0f
    for (unsigned i = 0; i < height; i++)
        memset(pixels->GetRow(i), 0, width * pixels->GetBytesPerPixel());
}

/** Converts 'srcImage' to 'destPixFmt'; ; result uses c_SimpleBuffer for storage.
    If 'destPixFmt' is the same as source image's pixel format, returns a copy of 'srcImg'. */
c_Image *c_Image::ConvertPixelFormat(c_Image &srcImg, PixelFormat_t destPixFmt)
{
    return ConvertPixelFormat(srcImg, destPixFmt,  0, 0, srcImg.GetWidth(), srcImg.GetHeight());
}

/// Converts the specified fragment of 'srcImage' to 'destPixFmt'; result uses c_SimpleBuffer for storage
c_Image *c_Image::ConvertPixelFormat(c_Image &srcImg, PixelFormat_t destPixFmt, unsigned x0, unsigned y0, unsigned width, unsigned height)
{
    assert(srcImg.GetPixelFormat() > PIX_UNCHANGED && srcImg.GetPixelFormat() < PIX_NUM_FORMATS);
    assert(!(destPixFmt == PIX_PAL8 && srcImg.GetPixelFormat() != PIX_PAL8));
    assert(x0 < srcImg.GetWidth());
    assert(y0 < srcImg.GetHeight());
    assert(x0 + width <= srcImg.GetWidth());
    assert(y0 + height <= srcImg.GetHeight());

    if (srcImg.GetPixelFormat() == destPixFmt)
    {
        if (x0 == 0 && y0 == 0 && width == srcImg.GetWidth() && height == srcImg.GetHeight())
        {
            return new c_Image(srcImg);
        }
        else // No conversion required, just copy the data
        {
            c_Image *destImg = new c_Image(width, height, destPixFmt);

            for (unsigned j = 0; j < height; j++)
                for (unsigned i = 0; i < height; i++)
                    memcpy((uint8_t *)destImg->GetRow(j) + i * BytesPerPixel[destPixFmt],
                           (uint8_t *)srcImg.GetRow(j + y0) + (i + x0) * BytesPerPixel[destPixFmt],
                           width * BytesPerPixel[destPixFmt]);

            return destImg;
        }
    }

    c_Image *destImg = new c_Image(width, height, destPixFmt);

    int inpPtrStep = BytesPerPixel[srcImg.GetPixelFormat()],
        outPtrStep = BytesPerPixel[destPixFmt];

    for (unsigned j = 0; j < height; j++)
    {
        uint8_t *inpPtr = (uint8_t*)srcImg.GetRow(j + y0) + x0 * BytesPerPixel[srcImg.GetPixelFormat()],
                *outPtr = (uint8_t*)destImg->GetRow(j);

        for (unsigned i = 0; i < width; i++)
        {
            if (srcImg.GetPixelFormat() == PIX_MONO8)
            {
                uint8_t src = *inpPtr;
                switch (destPixFmt)
                {
                case PIX_MONO16: *(uint16_t *)outPtr = (uint16_t)src << 8; break;
                case PIX_MONO32F: *(float *)outPtr = src * 1.0f/0xFF; break;
                case PIX_RGB8: outPtr[0] = outPtr[1] = outPtr[2] = src; break;
                case PIX_RGB16:
                    ((uint16_t *)outPtr)[0] =
                        ((uint16_t *)outPtr)[1] =
                        ((uint16_t *)outPtr)[2] =  (uint16_t)src << 8;
                    break;
                }
            }
            else if (srcImg.GetPixelFormat() == PIX_MONO16)
            {
                uint16_t src = *(uint16_t *)inpPtr;
                switch (destPixFmt)
                {
                case PIX_MONO8: *outPtr = (uint8_t)(src >> 8); break;
                case PIX_MONO32F: *(float *)outPtr = src * 1.0f/0xFFFF; break;
                case PIX_RGB8: outPtr[0] = outPtr[1] = outPtr[2] = (uint8_t)(src >> 8); break;
                case PIX_RGB16:
                    ((uint16_t *)outPtr)[0] =
                        ((uint16_t *)outPtr)[1] =
                        ((uint16_t *)outPtr)[2] = src;
                    break;
                }
            }
            else if (srcImg.GetPixelFormat() == PIX_MONO32F)
            {
                float src = *(float *)inpPtr;
                switch (destPixFmt)
                {
                case PIX_MONO8: *outPtr = (uint8_t)(src * 0xFF); break;
                case PIX_MONO16: *(uint16_t *)outPtr = (uint16_t)(src * 0xFFFF); break;
                case PIX_RGB8: outPtr[0] = outPtr[1] = outPtr[2] = (uint8_t)(src * 0xFF); break;
                case PIX_RGB16:
                    ((uint16_t *)outPtr)[0] =
                        ((uint16_t *)outPtr)[1] =
                        ((uint16_t *)outPtr)[2] = (uint16_t)(src * 0xFFFF); break;
                }
            }
            // When converting from a color format to mono, use sum (scaled) of all channels as the pixel brightness.
            else if (srcImg.GetPixelFormat() == PIX_PAL8)
            {
                uint8_t *palette = srcImg.GetPalette();
                uint8_t src = *inpPtr;
                switch (destPixFmt)
                {
                case PIX_MONO8: *outPtr = (uint8_t)(((int)palette[3*src] + palette[3*src+1] + palette[3*src+2])/3); break;
                case PIX_MONO16: *(uint16_t *)outPtr = ((uint16_t)palette[3*src] + palette[3*src+1] + palette[3*src+2])/3; break;
                case PIX_MONO32F: *(float *)outPtr = ((int)palette[3*src] + palette[3*src+1] + palette[3*src+2]) * 1.0f/(3*0xFF); break;
                case PIX_RGB8:
                    outPtr[0] = palette[3*src];
                    outPtr[1] = palette[3*src+1];
                    outPtr[2] = palette[3*src+2];
                    break;
                case PIX_RGB16:
                    ((uint16_t *)outPtr)[0] = (uint16_t)palette[3*src] << 8;
                    ((uint16_t *)outPtr)[1] = (uint16_t)palette[3*src+1] << 8;
                    ((uint16_t *)outPtr)[2] = (uint16_t)palette[3*src+2] << 8;
                    break;
                }
            }
            else if (srcImg.GetPixelFormat() == PIX_RGB8)
            {
                switch (destPixFmt)
                {
                case PIX_MONO8: *outPtr = (uint8_t)(((int)inpPtr[0] + inpPtr[1] + inpPtr[2])/3); break;
                case PIX_MONO16: *(uint16_t *)outPtr = ((uint16_t)inpPtr[0] + inpPtr[1] + inpPtr[2])/3; break;
                case PIX_MONO32F: *(float *)outPtr = ((int)inpPtr[0] + inpPtr[1] + inpPtr[2]) * 1.0f/(3*0xFF); break;
                case PIX_RGB16:
                    ((uint16_t *)outPtr)[0] = (uint16_t)inpPtr[0] << 8;
                    ((uint16_t *)outPtr)[1] = (uint16_t)inpPtr[1] << 8;
                    ((uint16_t *)outPtr)[2] = (uint16_t)inpPtr[2] << 8;
                    break;
                }
            }
            else if (srcImg.GetPixelFormat() == PIX_RGB16)
            {
                uint16_t *inpPtr16 = (uint16_t *)inpPtr;
                switch (destPixFmt)
                {
                case PIX_MONO8: *outPtr = (uint8_t)(((int)inpPtr16[0] + inpPtr16[1] + inpPtr16[2])/3); break;
                case PIX_MONO16: *(uint16_t *)outPtr = (uint16_t)(((int)inpPtr16[0] + inpPtr16[1] + inpPtr16[2])/3); break;
                case PIX_MONO32F: *(float *)outPtr = ((int)inpPtr16[0] + inpPtr16[1] + inpPtr16[2]) * 1.0f/(3*0xFFFF); break;
                case PIX_RGB8:
                    outPtr[0] = (uint8_t)(inpPtr16[0] >> 8);
                    outPtr[1] = (uint8_t)(inpPtr16[1] >> 8);
                    outPtr[2] = (uint8_t)(inpPtr16[2] >> 8);
                    break;
                }
            }

            inpPtr += inpPtrStep;
            outPtr += outPtrStep;
        }
    }

    return destImg;
}

void c_Image::SetPalette(uint8_t palette[])
{
    memcpy(this->palette.get(), palette, sizeof(this->palette));
}

/// Copies a rectangular area from 'src' to 'dest'. Pixel formats of 'src' and 'dest' have to be the same.
void c_Image::Copy(c_Image &src, c_Image &dest, unsigned srcX, unsigned srcY, unsigned width, unsigned height, unsigned destX, unsigned destY)
{
    assert(src.GetPixelFormat() == dest.GetPixelFormat());
    assert(srcX + width <= src.GetWidth());
    assert(srcY + height <= src.GetHeight());
    assert(destX + width <= dest.GetWidth());
    assert(destY + height <= dest.GetHeight());


    int bpp = BytesPerPixel[src.GetPixelFormat()];

    for (unsigned y = 0; y < height; y++)
        memcpy((uint8_t *)dest.GetRow(destY + y) + destX * bpp,
               (uint8_t *)src.GetRow(srcY + y) + srcX * bpp,
               width * bpp);
}

inline float ClampLuminance(float val, float maxVal)
{
    if (val < 0.0f)
        return 0.0f;
    else if (val > maxVal)
        return maxVal;
    else
        return val;
}

/// Cubic (Hermite) interpolation of 4 subsequent values fm1, f0, f1, f2 at location 0<=t<=1 between the middle elements (f0 and f1)
template<typename T>
inline float InterpolateCubic(float t, T fm1, T f0, T f1, T f2)
{
    float delta_k = (float)f1 - f0;
    float dk = ((float)f1 - fm1)*0.5f, dk1 = ((float)f2 - f0)*0.5f;

    float a0 = f0, a1 = dk, a2 = 3.0f*delta_k - 2.0f*dk - dk1,
        a3 = (float)dk + dk1 - 2.0f*delta_k;

    return t*(t*(a3*t + a2)+a1)+a0;
}

template<typename Lum_t>
void ResizeAndTranslateImpl(
    IImageBuffer &srcImg,
    IImageBuffer &destImg,
    int srcXmin,     ///< X min of input data in source image
    int srcYmin,     ///< Y min of input data in source image
    int srcXmax,     ///< X max of input data in source image
    int srcYmax,     ///< Y max of input data in source image
    float xOfs,      ///< X offset of input data in output image
    float yOfs,      ///< Y offset of input data in output image
    bool clearToZero ///< If 'true', 'destImg' areas not copied on will be cleared to zero
)
{
    int xOfsInt, yOfsInt;
    float temp;
    float xOfsFrac = std::modf(xOfs, &temp); xOfsInt = (int)temp;
    float yOfsFrac = std::modf(yOfs, &temp); yOfsInt = (int)temp;

    int bytesPP = BytesPerPixel[srcImg.GetPixelFormat()];

    // start and end (inclusive) coordinates to fill in the output buffer
    int destXstart = (xOfsInt < 0) ? 0 : xOfsInt;
    int destYstart = (yOfsInt < 0) ? 0 : yOfsInt;

    int destXend = std::min(xOfsInt + srcXmax, (int)destImg.GetWidth()-1);
    int destYend = std::min(yOfsInt + srcYmax, (int)destImg.GetHeight()-1);

    if (destYend < destYstart || destXend < destXstart)
    {
        if (clearToZero)
            for (unsigned y = 0; y < destImg.GetHeight(); y++)
                memset(destImg.GetRow(y), 0, destImg.GetWidth() * bytesPP);
        return;
    }

    if (clearToZero)
    {
        // Unchanged rows at the top
        for (int y = 0; y < destYstart; y++)
            memset(destImg.GetRow(y), 0, destImg.GetWidth() * bytesPP);
        // Unchanged rows at the bottom
        for (unsigned y = destYend + 1; y < destImg.GetHeight(); y++)
            memset(destImg.GetRow(y), 0, destImg.GetWidth() * bytesPP);
        for (int y = destYstart; y <= destYend; y++)
        {
            // Columns to the left of the target area
            memset(destImg.GetRow(y), 0, destXstart*bytesPP);
            // Columns to the right of the target area
            memset((uint8_t *)destImg.GetRow(y) + (destXend+1)*bytesPP, 0, (destImg.GetWidth() - 1 - destXend) * bytesPP);
        }
    }

    if (xOfsFrac == 0.0f && yOfsFrac == 0.0f)
    {
        // Not a subpixel translation; just copy the pixels line by line
        for (int y = destYstart; y <= destYend; y++)
        {
            memcpy((uint8_t *)destImg.GetRow(y) + destXstart * bytesPP,
                (uint8_t *)srcImg.GetRow(y - yOfsInt + srcYmin) + (destXstart - xOfsInt + srcXmin) * bytesPP,
                (destXend - destXstart + 1) * bytesPP);
        }
    }
    else
    {
        // Subpixel translation
        assert(srcImg.GetPixelFormat() != PIX_PAL8);

        // First, copy the 2-border pixels of the target area without change.

        // 2 top and 2 bottom rows
        for (int i = 0; i < 2; i++)
        {
            memcpy((uint8_t *)destImg.GetRow(destYstart + i) + destXstart*bytesPP,
                   (uint8_t *)srcImg.GetRow(destYstart + i - yOfsInt + srcYmin) + (destXstart - xOfsInt + srcXmin) * bytesPP,
                   (destXend - destXstart + 1) * bytesPP);

            memcpy((uint8_t *)destImg.GetRow(destYend - i) + destXstart*bytesPP,
                (uint8_t *)srcImg.GetRow(destYend - i - yOfsInt + srcYmin) + (destXstart - xOfsInt + srcXmin) * bytesPP,
                (destXend - destXstart + 1) * bytesPP);
        }

        // 2 leftmost and 2 rightmost columns
        for (int y = destYstart; y <= destYend; y++)
        {
            // 2 leftmost columns
            memcpy((uint8_t *)destImg.GetRow(y) + destXstart*bytesPP,
                   (uint8_t *)srcImg.GetRow(y - yOfsInt + srcYmin) + (destXstart - xOfsInt + srcXmin)*bytesPP,
                   2 * bytesPP); // copy 2 pixels

            // 2 rightmost columns
            memcpy((uint8_t *)destImg.GetRow(y) + (destXend-1)*bytesPP,
                   (uint8_t *)srcImg.GetRow(y - yOfsInt + srcYmin) + (destXend - 1 - xOfsInt + srcXmin)*bytesPP,
                   2 * bytesPP); // copy 2 pixels
        }

        // Second, perform cubic interpolation inside the target area.

        float maxLum = 0.0f;
        switch (srcImg.GetPixelFormat())
        {
        case PIX_MONO8:
        case PIX_RGB8:
        case PIX_RGBA8:
            maxLum = (float)0xFF; break;

        case PIX_MONO16:
        case PIX_RGB16:
        case PIX_RGBA16:
            maxLum = (float)0xFFFF; break;

        case PIX_MONO32F:
        case PIX_RGB32F:
        case PIX_RGBA32F:
            maxLum = 1.0f;
        }

        int idx = xOfsFrac < 0.0f ? 1 : -1;
        int idy = yOfsFrac < 0.0f ? 1 : -1;

        xOfsFrac = std::fabs(xOfsFrac);
        yOfsFrac = std::fabs(yOfsFrac);

        int numChannels = NumChannels[(int)srcImg.GetPixelFormat()];

        // Skip 2-pixels borders on each side of the image
        #pragma omp parallel for
        for (int row = destYstart+2; row <= destYend-2; row++)
        {
            for (int col = destXstart+2; col <= destXend-2; col++)
            {
                for (int ch = 0; ch < numChannels; ch++)
                {
                    float yvals[4];

                    // Perform 4 interpolations at 4 adjacent rows, using X offsets -1, 0, 1, 2 (*idx)
                    int y = row - idy;
                    int srcY = y - yOfsInt + srcYmin;
                    for (int relY = -1; relY <= 2; relY++)
                    {
                        int srcX = col - xOfsInt + srcXmin;
                        yvals[relY+1] = InterpolateCubic(xOfsFrac,
                            ((Lum_t *)srcImg.GetRow(srcY))[(srcX - idx)*numChannels + ch],
                            ((Lum_t *)srcImg.GetRow(srcY))[(srcX      )*numChannels + ch],
                            ((Lum_t *)srcImg.GetRow(srcY))[(srcX + idx)*numChannels + ch],
                            ((Lum_t *)srcImg.GetRow(srcY))[(srcX + idx + idx)*numChannels + ch]);

                        y += idy;
                        srcY += idy;
                    }

                    // Perform the final vertical (column) interpolation of the 4 horizontal (row) values interpolated previously
                    ((Lum_t *)destImg.GetRow(row))[col*numChannels + ch] = (Lum_t)ClampLuminance(InterpolateCubic(yOfsFrac, yvals[0], yvals[1], yvals[2], yvals[3]), maxLum);
                }
            }
        }
    }
}

/// Resizes and translates image (or its fragment) by cropping and/or padding (with zeros) to the destination size and offset (there is no scaling)
/** Subpixel translation (i.e. if 'xOfs' or 'yOfs' have a fractional part) of palettised images is not supported. */
void c_Image::ResizeAndTranslate(
    IImageBuffer &srcImg,
    IImageBuffer &destImg,
    int srcXmin,     ///< X min of input data in source image
    int srcYmin,     ///< Y min of input data in source image
    int srcXmax,     ///< X max of input data in source image
    int srcYmax,     ///< Y max of input data in source image
    float xOfs,      ///< X offset of input data in output image
    float yOfs,      ///< Y offset of input data in output image
    bool clearToZero ///< If 'true', 'destImg' areas not copied on will be cleared to zero
    )
{
    assert(srcImg.GetPixelFormat() == destImg.GetPixelFormat());

    switch (srcImg.GetPixelFormat())
    {
    case PIX_MONO8:
    case PIX_RGB8:
    case PIX_RGBA8:
        ResizeAndTranslateImpl<uint8_t>(srcImg, destImg, srcXmin, srcYmin, srcXmax, srcYmax, xOfs, yOfs, clearToZero); break;

    case PIX_MONO16:
    case PIX_RGB16:
    case PIX_RGBA16:
        ResizeAndTranslateImpl<uint16_t>(srcImg, destImg, srcXmin, srcYmin, srcXmax, srcYmax, xOfs, yOfs, clearToZero); break;

    case PIX_MONO32F:
    case PIX_RGB32F:
    case PIX_RGBA32F:
        ResizeAndTranslateImpl<float>(srcImg, destImg, srcXmin, srcYmin, srcXmax, srcYmax, xOfs, yOfs, clearToZero); break;
    }
}

void NormalizeFpImage(c_Image &img, float minLevel, float maxLevel)
{
    float lmin = FLT_MAX, lmax = -FLT_MAX; // min and max brightness in the input image
    for (unsigned row = 0; row < img.GetHeight(); row++)
        for (unsigned col = 0; col < img.GetWidth(); col++)
        {
            float val = ((float *)img.GetRow(row))[col];
            if (val < lmin)
                lmin = val;
            if (val > lmax)
                lmax = val;
        }

    // Determine coefficients 'a' and 'b' which satisfy: new_luminance := a * old_luminance + b
    float a = (maxLevel - minLevel) / (lmax - lmin);
    float b = maxLevel - a*lmax;

    // Pixels with brightness 'minLevel' become black and those of 'maxLevel' become white.

    for (unsigned row = 0; row < img.GetHeight(); row++)
        for (unsigned col = 0; col < img.GetWidth(); col++)
        {
            float &val = ((float *)img.GetRow(row))[col];
            val = a * val + b;
            if (val < 0.0f)
                val = 0.0f;
            else if (val > 1.0f)
                val = 1.0f;
        }
}

#if USE_CFITSIO
/// Loads an image from a FITS file; the result's pixel format will be PIX_MONO8, PIX_MONO16 or PIX_MONO32F
c_Image *LoadFitsImage(std::string fname)
{
    c_Image *result = 0;
    fitsfile *fptr;
    int status = 0;
    long dimensions[3] = { 0 };
    int naxis;
    int bitsPerPixel;
    int imgIndex = 0;
    while (0 == status)
    {
        // The i-th image in FITS file is accessed by appending [i] to file name; try image [0] first
        fits_open_file(&fptr, boost::str(boost::format("%s[%d]") % fname % imgIndex).c_str(), READONLY, &status);
        fits_read_imghdr(fptr, 3, 0, &bitsPerPixel, &naxis, dimensions, 0, 0, 0, &status);
        if (0 == status && (naxis > 3 || naxis <= 0))
        {
            // Try opening a subsequent image; sometimes image [0] has 0 size (e.g. in some files from SDO)
            fits_close_file(fptr, &status);
            imgIndex++;
            continue;
        }
        else if (status)
        {
            fits_close_file(fptr, &status);
            return 0;
        }
        else
            break;

        //TODO: if 3 axes are detected, convert RGB to grayscale; now we only load the first channel
    }

    if (dimensions[0] < 0 || dimensions[1] < 0)
        return 0;

    size_t numPixels = dimensions[0]*dimensions[1];
    void *fitsPixels = 0;
    int destType; // data type that the pixels will be converted to on read
    PixelFormat_t pixFmt;

    switch (bitsPerPixel)
    {
    case BYTE_IMG:
        fitsPixels = operator new[](numPixels * sizeof(uint8_t));
        destType = TBYTE;
        pixFmt = PIX_MONO8;
        break;

    case SHORT_IMG:
        fitsPixels = operator new[](numPixels * sizeof(uint16_t));
        destType = TUSHORT;
        pixFmt = PIX_MONO16;
        break;

    default:
        // all the remaining types will be converted to 32-bit floating-point
        fitsPixels = operator new[](numPixels * sizeof(float));
        destType = TFLOAT;
        pixFmt = PIX_MONO32F;
        break;
    }

    fits_read_img(fptr, destType, 1, numPixels, 0, fitsPixels, 0, &status);

    if (NUM_OVERFLOW == status && (bitsPerPixel == BYTE_IMG || bitsPerPixel == SHORT_IMG))
    {
        // Input file had some negative values; let us just load it as floating-point
        status = 0;
        operator delete[](fitsPixels);
        fitsPixels = operator new[](numPixels * sizeof(float));
        destType = TFLOAT;
        pixFmt = PIX_MONO32F;
        fits_read_img(fptr, destType, 1, numPixels, 0, fitsPixels, 0, &status);
    }

    fits_close_file(fptr, &status);

    if (!status)
    {
        if (destType == TFLOAT)
        {
            // If any value is < 0, set it to 0. If all remaining values are <= 1.0,
            // leave them unchanged. If the maximum value is > 1.0, scale everything down
            // so that maximum is 1.0.

            float maxval = 0.0f;
            for (unsigned long y = 0; y < (unsigned long)dimensions[1]; y++)
                for (unsigned long  x = 0; x < (unsigned long)dimensions[0]; x++)
                {
                    float &val = ((float *)fitsPixels)[x + y*dimensions[0]];
                    if (val < 0.0f)
                        val = 0.0f;
                    else if (val > maxval)
                        maxval = val;
                }

            if (maxval > 1.0f)
            {
                float maxvalinv = 1.0f/maxval;
                for (int y = 0; y < dimensions[1]; y++)
                    for (int x = 0; x < dimensions[0]; x++)
                        ((float *)fitsPixels)[x + y*dimensions[0]] *= maxvalinv;
            }
        }

        result = new c_Image(dimensions[0], dimensions[1], pixFmt);
        for (unsigned row = 0; row < result->GetHeight(); row++)
            memcpy(result->GetRow(row), (uint8_t *)fitsPixels + row*dimensions[0]*BytesPerPixel[pixFmt], dimensions[0]*BytesPerPixel[pixFmt]);
    }
    operator delete[](fitsPixels);

    return result;
}
#endif

c_Image *LoadImageAs(
    std::string fname,        ///< Full path (including file name and extension)
    std::string extension,    ///< lowercase extension
    PixelFormat_t destFmt,    ///< Pixel format of the result; can be one of PIX_MONO8, PIX_MONO32F
    std::string *errorMsg     ///< If not null, may receive an error message (if any)
    )
{
    c_Image *result = 0;

#if USE_CFITSIO
    if (extension == "fit" || extension == "fits")
    {
        result = LoadFitsImage(fname);
        if (result)
        {
            c_Image *converted = c_Image::ConvertPixelFormat(*result, destFmt);
            delete result;
            result = converted;
        }
    }
    else
#endif
    {
#if USE_FREEIMAGE
        //TODO: add handling of FreeImage's error message (if any)

        FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
        fif = FreeImage_GetFileType(fname.c_str());
        if (FIF_UNKNOWN == fif)
        {
            fif = FreeImage_GetFIFFromFilename(fname.c_str());
        }

        if (fif != FIF_UNKNOWN && FreeImage_FIFSupportsReading(fif))
        {
            FIBITMAP *fibmp = FreeImage_Load(fif, fname.c_str());
            if (!fibmp)
                return 0;

            FIBITMAP *fibmpConv = 0;

            switch (destFmt)
            {
            case PIX_MONO8:
                {
                    // Currently this it the only universal way in FreeImage to convert ANY bitmap type to grayscale
                    FIBITMAP *fiBmpFloat = FreeImage_ConvertToFloat(fibmp);
                    fibmpConv = FreeImage_ConvertToStandardType(fiBmpFloat);
                    FreeImage_Unload(fiBmpFloat);
                }
                break;

            case PIX_MONO32F: fibmpConv = FreeImage_ConvertToFloat(fibmp); break;
            }

            FreeImage_Unload(fibmp);
            if (!fibmpConv)
                return 0;

            result = new c_Image(FreeImage_GetWidth(fibmpConv),
                FreeImage_GetHeight(fibmpConv),
                destFmt);
            for (unsigned row = 0; row < result->GetHeight(); row++)
                memcpy(result->GetRow(result->GetHeight() - 1 - row), FreeImage_GetScanLine(fibmpConv, row), result->GetWidth() * BytesPerPixel[destFmt]);

            FreeImage_Unload(fibmpConv);
        }
#else
        c_Image *newImg = 0;
        if (extension == "tif" || extension == "tiff")
            newImg = ReadTiff(fname.c_str(), errorMsg);
        else if (extension == "bmp")
            newImg = ReadBmp(fname.c_str());

        if (newImg == 0)
        {
            return 0;
        }
        else
        {
            result = c_Image::ConvertPixelFormat(*newImg, destFmt);
            delete newImg;
        }
#endif
    }

    return result;
}

/// Loads the specified image file and converts it to PIX_MONO32F; returns 0 on error
c_Image *LoadImageFileAsMono32f(
    std::string fname,        ///< Full path (including file name and extension)
    std::string extension,    ///< lowercase extension
    std::string *errorMsg    ///< If not null, may receive an error message (if any)
    )
{
    return LoadImageAs(fname, extension, PIX_MONO32F, errorMsg);
}

/// Loads the specified image file and converts it to PIX_MONO8; returns 0 on error
c_Image *LoadImageFileAsMono8(
    std::string fname,        ///< Full path (including file name and extension)
    std::string extension,    ///< lowercase extension
    std::string *errorMsg    ///< If not null, may receive an error message (if any)
    )
{
    return LoadImageAs(fname, extension, PIX_MONO8, errorMsg);
}


/// Saves image using the specified output format
bool SaveImageFile(
    std::string fname, ///< Full destination path
    c_Image &img,
    OutputFormat_t outputFmt
    )
{
    bool result = false;

#if USE_CFITSIO
    if (outputFmt == OUTF_FITS_32F || outputFmt == OUTF_FITS_16 || outputFmt == OUTF_FITS_8)
    {
        fitsfile *fptr;
        int status;
        long dimensions[2] = { img.GetWidth(), img.GetHeight() };
        void *array = 0; // contents of the output FITS file
        if (outputFmt == OUTF_FITS_32F)
            array = operator new[](dimensions[0] * dimensions[1] * sizeof(float));
        else if (outputFmt == OUTF_FITS_16)
            array = operator new[](dimensions[0] * dimensions[1] * sizeof(uint16_t));
        else if (outputFmt == OUTF_FITS_8)
            array = operator new[](dimensions[0] * dimensions[1] * sizeof(uint8_t));
        if (!array)
            return false;

        switch (outputFmt)
        {
        case OUTF_FITS_32F:
            for (unsigned row = 0; row < img.GetHeight(); row++)
                memcpy((uint8_t *)array + row*img.GetWidth()*sizeof(float), img.GetRow(row), img.GetWidth()*sizeof(float));
            break;

        case OUTF_FITS_16:
            {
                c_Image *img16 = c_Image::ConvertPixelFormat(img, PIX_MONO16);
                for (unsigned row = 0; row < img16->GetHeight(); row++)
                    memcpy((uint8_t *)array + row*img16->GetWidth()*sizeof(uint16_t), img16->GetRow(row), img16->GetWidth()*sizeof(uint16_t));

                delete img16;
                break;
            }

        case OUTF_FITS_8:
            {
                c_Image *img8 = c_Image::ConvertPixelFormat(img, PIX_MONO8);
                for (unsigned row = 0; row < img8->GetHeight(); row++)
                    memcpy((uint8_t *)array + row*img8->GetWidth()*sizeof(uint8_t), img8->GetRow(row), img8->GetWidth()*sizeof(uint8_t));

                delete img8;
                break;
            }
        }


        status = 0;
        fits_create_file(&fptr, (std::string("!") + fname).c_str(), &status); // A leading "!" overwrites an existing file
        int bitPix, datatype;
        if (outputFmt == OUTF_FITS_32F)
        {
            bitPix = FLOAT_IMG;
            datatype = TFLOAT;
        }
        else if (outputFmt == OUTF_FITS_16)
        {
            bitPix = USHORT_IMG;
            datatype = TUSHORT;
        }
        else if (outputFmt == OUTF_FITS_8)
        {
            bitPix = BYTE_IMG;
            datatype = TBYTE;
        }
        fits_create_img(fptr, bitPix, 2, dimensions, &status);
        fits_write_history(fptr, "Processed in ImPPG.", &status);
        fits_write_img(fptr, datatype, 1, dimensions[0]*dimensions[1], array, &status);

        operator delete[](array);
        fits_close_file(fptr, &status);
        return (status == 0);
    }
#endif

#if USE_FREEIMAGE

    FIBITMAP *outputBmp = 0;
    c_Image *convertedImg = 0;

    switch (outputFmt)
    {
    case OUTF_BMP_MONO_8:
    case OUTF_PNG_MONO_8:
    case OUTF_TIFF_MONO_8_LZW:
        outputBmp = FreeImage_AllocateT(FIT_BITMAP, img.GetWidth(), img.GetHeight(), 8);
        convertedImg = c_Image::ConvertPixelFormat(img, PIX_MONO8);
        break;

    case OUTF_TIFF_MONO_16:
    case OUTF_TIFF_MONO_16_ZIP:
        outputBmp = FreeImage_AllocateT(FIT_UINT16, img.GetWidth(), img.GetHeight());
        convertedImg = c_Image::ConvertPixelFormat(img, PIX_MONO16);
        break;

    case OUTF_TIFF_MONO_32F:
    case OUTF_TIFF_MONO_32F_ZIP:
        outputBmp = FreeImage_AllocateT(FIT_FLOAT, img.GetWidth(), img.GetHeight());
        convertedImg = &img;
        break;
    }

    if (!outputBmp || !convertedImg)
        return false;

    for (unsigned row = 0; row < img.GetHeight(); row++)
        memcpy(FreeImage_GetScanLine(outputBmp, row),
            convertedImg->GetRow(convertedImg->GetHeight() - 1 - row),
            convertedImg->GetWidth() * convertedImg->GetBuffer().GetBytesPerPixel());

    if (convertedImg != &img)
        delete convertedImg;

    switch (outputFmt)
    {
    case OUTF_BMP_MONO_8:
        result = FreeImage_Save(FIF_BMP, outputBmp, fname.c_str(), BMP_DEFAULT);
        break;

    case OUTF_PNG_MONO_8:
        result = FreeImage_Save(FIF_PNG, outputBmp, fname.c_str(), PNG_DEFAULT);
        break;

    case OUTF_TIFF_MONO_8_LZW:
        result = FreeImage_Save(FIF_TIFF, outputBmp, fname.c_str(), TIFF_LZW);
        break;

    case OUTF_TIFF_MONO_16:
        result = FreeImage_Save(FIF_TIFF, outputBmp, fname.c_str(), TIFF_NONE);
        break;

    case OUTF_TIFF_MONO_16_ZIP:
        result = FreeImage_Save(FIF_TIFF, outputBmp, fname.c_str(), TIFF_DEFLATE);
        break;

    case OUTF_TIFF_MONO_32F:
        result = FreeImage_Save(FIF_TIFF, outputBmp, fname.c_str(), TIFF_NONE);
        break;

    case OUTF_TIFF_MONO_32F_ZIP:
        result = FreeImage_Save(FIF_TIFF, outputBmp, fname.c_str(), TIFF_DEFLATE);
        break;
    }

    FreeImage_Unload(outputBmp);

#else

    switch (outputFmt)
    {
    case OUTF_BMP_MONO_8:
        {
            c_Image *outputImg = c_Image::ConvertPixelFormat(img, PIX_MONO8);
            result = SaveBmp(fname.c_str(), *outputImg);
            delete outputImg;
            break;
        }

    case OUTF_TIFF_MONO_16:
        {
            c_Image *outputImg = c_Image::ConvertPixelFormat(img, PIX_MONO16);
            result = SaveTiff(fname.c_str(), *outputImg);
            delete outputImg;
            break;
        }
    }
#endif

    return result;
}

/// Multiply by another image; both images have to be PIX_MONO32F and have the same dimensions
void c_Image::Multiply(c_Image &mult)
{
    assert(GetPixelFormat() == PIX_MONO32F && mult.GetPixelFormat() == PIX_MONO32F);
    assert(GetWidth() == mult.GetWidth() && GetHeight() == mult.GetHeight());

    for (unsigned row = 0; row < GetHeight(); row++)
        for (unsigned col = 0; col < GetWidth(); col++)
            ((float *)GetRow(row))[col] *= ((float *)mult.GetRow(row))[col];
}

/// Returns 'true' if image's width and height were successfully read; returns 'false' on error
bool GetImageSize(
    std::string fname,     ///< Full path (including file name and extension)
    std::string extension, ///< lowercase extension
    unsigned &width,            ///< Receives image width
    unsigned &height            ///< Receives image height
)
{
#if USE_CFITSIO
    if (extension == "fit" || extension == "fits")
    {
        fitsfile *fptr;
        int status = 0;
        fits_open_file(&fptr, fname.c_str(), READONLY, &status);
        long dimensions[2] = { 0 }; //TODO: support 3-axis files too
        int naxis;
        fits_read_imghdr(fptr, 2, 0, 0, &naxis, dimensions, 0, 0, 0, &status);
        fits_close_file(fptr, &status);

        if (naxis != 2 || status)
            return false;
        else
        {
            width = dimensions[0];
            height = dimensions[1];
            return true;
        }
    }
#endif
#if USE_FREEIMAGE
    FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
    fif = FreeImage_GetFileType(fname.c_str());
    if (FIF_UNKNOWN == fif)
    {
        fif = FreeImage_GetFIFFromFilename(fname.c_str());
    }

    if (fif != FIF_UNKNOWN && FreeImage_FIFSupportsReading(fif))
    {
        FIBITMAP *fibmp = FreeImage_Load(fif, fname.c_str(), FIF_LOAD_NOPIXELS);
        if (!fibmp)
            return false;

        width = FreeImage_GetWidth(fibmp);
        height = FreeImage_GetHeight(fibmp);

        FreeImage_Unload(fibmp);
        return true;
    }
    else
        return false;
#else

    if (extension == "tif" || extension == "tiff")
        return GetTiffDimensions(fname.c_str(), width, height);
    else if (extension == "bmp")
        return GetBmpDimensions(fname.c_str(), width, height);
    else
        return false;

#endif
}
