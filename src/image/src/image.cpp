/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2022 Filip Szczerek <ga.software@yahoo.com>

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

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <optional>
#include <boost/format.hpp>

#include "common/imppg_assert.h"
#include "image/image.h"
#if (USE_FREEIMAGE)
  #include "FreeImage.h"
  #ifdef __APPLE__
    #undef _WINDOWS_
  #endif
#else
  #include "bmp.h"
  #include "tiff.h"
#endif

#if USE_CFITSIO
#include <fitsio.h>
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
    static uint8_t* ptr = reinterpret_cast<uint8_t*>(&value16);
    return (ptr[0] == 0x11);
}

static std::string GetExtension(const std::string& filePath)
{
    const auto extension = std::filesystem::path(filePath).extension().string();
    if (extension.size() >= 1 && extension[0] == '.')
    {
        return extension.substr(1);
    }
    else
    {
        return extension;
    }
}

#if USE_CFITSIO
// Only saving as mono is supported.
static bool SaveAsFits(const IImageBuffer& buf, const std::string& fname)
{
    IMPPG_ASSERT(NumChannels[static_cast<size_t>(buf.GetPixelFormat())] == 1);

    fitsfile* fptr{nullptr};
    long dimensions[2] = { static_cast<long>(buf.GetWidth()), static_cast<long>(buf.GetHeight()) };
    const auto num_pixels = buf.GetWidth() * buf.GetHeight();
    // contents of the output FITS file
    auto array = std::make_unique<uint8_t[]>(num_pixels * buf.GetBytesPerPixel());

    const auto row_filler = [&](size_t bytesPerPixel)
    {
        for (unsigned row = 0; row < buf.GetHeight(); row++)
            memcpy(array.get() + row * buf.GetWidth() * bytesPerPixel, buf.GetRow(row), buf.GetWidth() * bytesPerPixel);
    };

    row_filler(buf.GetBytesPerPixel());

    int status = 0;
    fits_create_file(&fptr, (std::string("!") + fname).c_str(), &status); // a leading "!" overwrites an existing file

    int bitPix, datatype;
    switch (buf.GetPixelFormat())
    {
    case PixelFormat::PIX_MONO32F:
        bitPix = FLOAT_IMG;
        datatype = TFLOAT;
        break;
    case PixelFormat::PIX_MONO16:
        bitPix = USHORT_IMG;
        datatype = TUSHORT;
        break;
    case PixelFormat::PIX_MONO8:
        bitPix = BYTE_IMG;
        datatype = TBYTE;
        break;

    default: IMPPG_ABORT();
    }

    fits_create_img(fptr, bitPix, 2, dimensions, &status);
    fits_write_history(fptr, "Processed in ImPPG.", &status);
    fits_write_img(fptr, datatype, 1, dimensions[0] * dimensions[1], array.get(), &status);

    fits_close_file(fptr, &status);
    return (status == 0);
}
#endif // if USE_CFITSIO

#if USE_FREEIMAGE

c_FreeImageHandleWrapper::c_FreeImageHandleWrapper(c_FreeImageHandleWrapper&& rhs) noexcept
{
    *this = std::move(rhs);
}

c_FreeImageHandleWrapper& c_FreeImageHandleWrapper::operator=(c_FreeImageHandleWrapper&& rhs) noexcept
{
    m_FiBmp = rhs.m_FiBmp;
    rhs.m_FiBmp = nullptr;
    return *this;
}

void c_FreeImageHandleWrapper::reset(FIBITMAP* ptr)
{
    FreeImage_Unload(m_FiBmp);
    m_FiBmp = ptr;
}

c_FreeImageHandleWrapper::~c_FreeImageHandleWrapper()
{
    FreeImage_Unload(m_FiBmp);
}

std::optional<std::unique_ptr<IImageBuffer>> c_FreeImageBuffer::Create(c_FreeImageHandleWrapper&& fiBmp)
{
    PixelFormat pixFmt;
    switch (FreeImage_GetImageType(fiBmp.get()))
    {
    case FIT_BITMAP:
        {
            const auto colorType = FreeImage_GetColorType(fiBmp.get());
            const auto bytesPerPixel = FreeImage_GetBPP(fiBmp.get()) / 8;

            if (FIC_RGB == colorType || FIC_RGBALPHA == colorType)
            {
                const bool swapRGB = [&]() {
                    // as of FreeImage 3.18.0, 8-bit RGB(A) is always stored as BGR(A); check anyway
                    const auto redMask = FreeImage_GetRedMask(fiBmp.get());
                    const auto greenMask = FreeImage_GetGreenMask(fiBmp.get());
                    const auto blueMask = FreeImage_GetBlueMask(fiBmp.get());
                    if (redMask == 0xFF0000 && greenMask == 0x00FF00 && blueMask == 0x0000FF)
                    {
                        return true;
                    }
                    else if (redMask == 0        && greenMask == 0        && blueMask == 0 ||
                             redMask == 0x0000FF && greenMask == 0x00FF00 && blueMask == 0xFF0000)
                    {
                        return false;
                    }
                    else
                    {
                        IMPPG_ABORT_MSG("unexpected FreeImage RGB mask value(s)");
                    }
                }();

                switch (bytesPerPixel)
                {
                case 3: pixFmt = swapRGB ? PixelFormat::PIX_BGR8 : PixelFormat::PIX_RGB8; break;
                case 4: pixFmt = swapRGB ? PixelFormat::PIX_BGRA8 : PixelFormat::PIX_RGBA8; break;
                case 6: pixFmt = PixelFormat::PIX_RGB16; break;
                case 8: pixFmt = PixelFormat::PIX_RGBA16; break;
                default: return std::nullopt;
                }
            }
            else if (FIC_MINISBLACK == colorType)
            {
                switch (bytesPerPixel)
                {
                case 1: pixFmt = PixelFormat::PIX_MONO8; break;
                case 2: pixFmt = PixelFormat::PIX_MONO16; break;
                default: return std::nullopt;
                }
            }
            else
            {
                return std::nullopt;
            }

            break;
        }

    case FIT_UINT16: pixFmt = PixelFormat::PIX_MONO16; break;
    case FIT_FLOAT:  pixFmt = PixelFormat::PIX_MONO32F; break;
    case FIT_RGB16:  pixFmt = PixelFormat::PIX_RGB16; break;
    case FIT_RGBA16: pixFmt = PixelFormat::PIX_RGBA16; break;
    case FIT_RGBF:   pixFmt = PixelFormat::PIX_RGB32F; break;
    case FIT_RGBAF:  pixFmt = PixelFormat::PIX_RGBA32F; break;
    default: return std::nullopt;
    }

    std::unique_ptr<IImageBuffer> result(
        new c_FreeImageBuffer(
            std::move(fiBmp),
            FreeImage_GetBits(fiBmp.get()),
            pixFmt,
            FreeImage_GetPitch(fiBmp.get())
        )
    );

    if (RGBQUAD* fiPal = FreeImage_GetPalette(fiBmp.get()))
    {
        Palette& palette = result->GetPalette();
        for (int i = 0; i < 256; i++)
        {
            palette[i*3]   = fiPal[i].rgbRed;
            palette[i*3+1] = fiPal[i].rgbGreen;
            palette[i*3+2] = fiPal[i].rgbBlue;
        }
    }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wredundant-move"
    return std::move(result); // `std::move` needed for GCC earlier than v. 8
#pragma GCC diagnostic pop
}

unsigned c_FreeImageBuffer::GetWidth() const { return FreeImage_GetWidth(m_FiBmp.get()); }

unsigned c_FreeImageBuffer::GetHeight() const { return FreeImage_GetHeight(m_FiBmp.get()); }

size_t c_FreeImageBuffer::GetBytesPerRow() const { return FreeImage_GetLine(m_FiBmp.get()); }

size_t c_FreeImageBuffer::GetBytesPerPixel() const { return FreeImage_GetBPP(m_FiBmp.get()) / 8; }

static std::tuple<FREE_IMAGE_FORMAT, int> GetFiFormatAndFlags(OutputFileType outpFileType)
{
#if USE_CFITSIO
    IMPPG_ASSERT(outpFileType != OutputFileType::FITS);
#endif

    switch (outpFileType)
    {
    case OutputFileType::BMP: return { FIF_BMP, BMP_DEFAULT };

    case OutputFileType::PNG: return { FIF_PNG, PNG_DEFAULT };

    case OutputFileType::TIFF: return { FIF_TIFF, TIFF_NONE };

    case OutputFileType::TIFF_COMPR_LZW: return { FIF_TIFF, TIFF_LZW };

    case OutputFileType::TIFF_COMPR_ZIP: return { FIF_TIFF, TIFF_DEFLATE };

    default: IMPPG_ABORT();
    }
}

static bool SaveAsFreeImage(const IImageBuffer& buf, const std::string& fname, OutputFileType outpFileType)
{
#if USE_CFITSIO
    IMPPG_ASSERT(outpFileType != OutputFileType::FITS);
#endif
    IMPPG_ASSERT(buf.GetPixelFormat() != PixelFormat::PIX_PAL8);

    bool result = false;

    FREE_IMAGE_TYPE fiType = FIT_UNKNOWN;
    int fiBpp = 8;
    switch (buf.GetPixelFormat())
    {
    case PixelFormat::PIX_MONO8:
        fiType = FIT_BITMAP;
        fiBpp = 8;
        break;

    case PixelFormat::PIX_RGB8:
        fiType = FIT_BITMAP;
        fiBpp = 24;
        break;

    case PixelFormat::PIX_RGBA8:
        fiType = FIT_BITMAP;
        fiBpp = 32;
        break;

    case PixelFormat::PIX_MONO16:
        fiType = FIT_UINT16;
        break;

    case PixelFormat::PIX_RGB16:
        fiType = FIT_RGB16;
        break;

    case PixelFormat::PIX_RGBA16:
        fiType = FIT_RGBA16;
        break;

    case PixelFormat::PIX_MONO32F:
        fiType = FIT_FLOAT;
        break;

    case PixelFormat::PIX_RGB32F:
        fiType = FIT_RGBF;
        break;

    case PixelFormat::PIX_RGBA32F:
        fiType = FIT_RGBAF;
        break;

    default: IMPPG_ABORT();
    }

    //FIXME! static_assert(false, "FIXME: wrong channel order when saving RGB after Combine");

    c_FreeImageHandleWrapper outputFiBmp = FreeImage_AllocateT(
        fiType, buf.GetWidth(), buf.GetHeight(), fiBpp
    );

    if (!outputFiBmp)
        return false;

    if (FIT_BITMAP == fiType && 24 == fiBpp)
    {
        for (unsigned row = 0; row < buf.GetHeight(); row++)
        {
            const auto* srcRow = static_cast<const std::uint8_t*>(buf.GetRow(buf.GetHeight() - 1 - row));
            std::uint8_t* destRow = FreeImage_GetScanLine(outputFiBmp.get(), row);
            for (unsigned x = 0; x < buf.GetWidth(); ++x)
            {
                destRow[3 * x + 0] = srcRow[3 * x + 2];
                destRow[3 * x + 1] = srcRow[3 * x + 1];
                destRow[3 * x + 2] = srcRow[3 * x + 0];
            }
        }
    }
    else
    {
        for (unsigned row = 0; row < buf.GetHeight(); row++)
        {
            memcpy(
                FreeImage_GetScanLine(outputFiBmp.get(), row),
                buf.GetRow(buf.GetHeight() - 1 - row),
                buf.GetWidth() * buf.GetBytesPerPixel()
            );
        }
    }

    const auto [fiFmt, fiFlags] = GetFiFormatAndFlags(outpFileType);
    result = FreeImage_Save(fiFmt, outputFiBmp.get(), fname.c_str(), fiFlags);

    return result;
}
#endif // if USE_FREEIMAGE


/// Simple image buffer; pixels are stored in row-major order with no padding.
class c_SimpleBuffer: public IImageBuffer
{
    PixelFormat m_PixFmt;
    unsigned m_Width, m_Height;
    size_t m_BytesPerPixel;
    std::unique_ptr<uint8_t[]> m_Pixels;
    Palette m_Palette{};

public:
    c_SimpleBuffer(int width, int height, PixelFormat pixFmt)
    : m_PixFmt(pixFmt),
      m_Width(width),
      m_Height(height),
      m_BytesPerPixel(BytesPerPixel[static_cast<size_t>(pixFmt)])
    {
        m_Pixels.reset(new uint8_t[m_Width * m_Height * m_BytesPerPixel]);
    }

    c_SimpleBuffer(const IImageBuffer& src)
    : m_PixFmt(src.GetPixelFormat()),
      m_Width(src.GetWidth()),
      m_Height(src.GetHeight()),
      m_BytesPerPixel(src.GetBytesPerPixel())
    {
        m_Pixels.reset(new uint8_t[m_Width * m_Height * m_BytesPerPixel]);
        for (unsigned row = 0; row < m_Height; ++row)
            memcpy(m_Pixels.get() + row * GetBytesPerRow(), src.GetRow(row), GetBytesPerRow());

        memcpy(&m_Palette, &src.GetPalette(), sizeof(m_Palette));
    }

    unsigned GetWidth() const override { return m_Width; }

    unsigned GetHeight() const override { return m_Height; }

    size_t GetBytesPerRow() const override { return m_Width * m_BytesPerPixel; }

    size_t GetBytesPerPixel() const override { return m_BytesPerPixel; }

    void* GetRow(size_t row) override { return m_Pixels.get() + row * m_Width * m_BytesPerPixel; }

    const void* GetRow(size_t row) const override { return m_Pixels.get() + row * m_Width * m_BytesPerPixel; }

    PixelFormat GetPixelFormat() const override { return m_PixFmt; }

    Palette& GetPalette() override { return m_Palette; }

    const Palette& GetPalette() const override { return m_Palette; }

    std::unique_ptr<IImageBuffer> GetCopy() const override
    {
        std::unique_ptr<c_SimpleBuffer> copy(new c_SimpleBuffer(m_Width, m_Height, m_PixFmt));
        memcpy(copy->m_Pixels.get(), m_Pixels.get(), m_Width * m_Height * m_BytesPerPixel);
        memcpy(&copy->m_Palette, &m_Palette, sizeof(m_Palette));
        return copy;
    }

private:
    bool SaveToFile(const std::string& fname, OutputFileType outpFileType) const override
    {
#if USE_CFITSIO
        if (outpFileType == OutputFileType::FITS)
        {
            return SaveAsFits(*this, fname);
        }
#endif

#if USE_FREEIMAGE
        return SaveAsFreeImage(*this, fname, outpFileType);
#else
        switch (outpFileType)
        {
            case OutputFileType::BMP: return SaveBmp(fname.c_str(), *this);
            case OutputFileType::TIFF: return SaveTiff(fname.c_str(), *this);
            default: IMPPG_ABORT();
        }
#endif
    }
};

#if USE_FREEIMAGE
bool c_FreeImageBuffer::SaveToFile(const std::string& fname, OutputFileType outpFileType) const
{
#if USE_CFITSIO
    if (outpFileType == OutputFileType::FITS)
    {
        return SaveAsFits(*this, fname);
    }
#endif

    const auto [fiFormat, fiFlags] = GetFiFormatAndFlags(outpFileType);
    return FreeImage_Save(fiFormat, m_FiBmp.get(), fname.c_str(), fiFlags);
}
#endif

/// Allocates memory for pixel data
c_Image::c_Image(unsigned width, unsigned height, PixelFormat pixFmt)
{
    m_Buffer = std::make_unique<c_SimpleBuffer>(width, height, pixFmt);
}

c_Image& c_Image::operator=(const c_Image& img)
{
    c_Image newImg(img);
    *this = std::move(newImg);

    return *this;
}

/// Performs a deep copy of 'img'
c_Image::c_Image(const c_Image& img)
{
    m_Buffer = img.m_Buffer->GetCopy();
}

/// Clears all pixels to zero value
void c_Image::ClearToZero()
{
    // works also for an array of floats; 32 zero bits represent a floating-point 0.0f
    const unsigned w = GetWidth();
    const unsigned h = GetHeight();
    for (unsigned i = 0; i < h; i++)
        memset(m_Buffer->GetRow(i), 0, w * m_Buffer->GetBytesPerPixel());
}

template<typename Src, typename Dest, typename ConversionFunc>
void ConvertWholeLine(
    ConversionFunc conversionOperations,
    unsigned width,
    const uint8_t* inPtr,
    int inPtrStep,
    uint8_t* outPtr,
    int outPtrStep
)
{
    for (unsigned i = 0; i < width; i++)
    {
        const auto* src = reinterpret_cast<const Src*>(inPtr);
        conversionOperations(src, reinterpret_cast<Dest*>(outPtr));
        inPtr += inPtrStep;
        outPtr += outPtrStep;
    }
}

static c_SimpleBuffer GetConvertedPixelFormatFragment(
    const IImageBuffer& srcBuf,
    PixelFormat destPixFmt,
    unsigned x0,
    unsigned y0,
    unsigned width,
    unsigned height
)
{
    IMPPG_ASSERT(!(destPixFmt == PixelFormat::PIX_PAL8 && srcBuf.GetPixelFormat() != PixelFormat::PIX_PAL8));
    IMPPG_ASSERT(x0 < srcBuf.GetWidth());
    IMPPG_ASSERT(y0 < srcBuf.GetHeight());
    IMPPG_ASSERT(x0 + width <= srcBuf.GetWidth());
    IMPPG_ASSERT(y0 + height <= srcBuf.GetHeight());

    if (srcBuf.GetPixelFormat() == destPixFmt)
    {
        if (x0 == 0 && y0 == 0 && width == srcBuf.GetWidth() && height == srcBuf.GetHeight())
        {
            return c_SimpleBuffer(srcBuf);
        }
        else // no conversion required, just copy the data
        {
            c_SimpleBuffer result(width, height, destPixFmt);

            auto bpp = result.GetBytesPerPixel();
            for (unsigned j = 0; j < height; j++)
                for (unsigned i = 0; i < width; i++)
                    memcpy(result.GetRowAs<uint8_t>(j),
                           srcBuf.GetRowAs<uint8_t>(j + y0) + (i + x0) * bpp,
                           width * bpp);

            return result;
        }
    }

    c_SimpleBuffer destBuf(width, height, destPixFmt);

    int inPtrStep = srcBuf.GetBytesPerPixel(),
        outPtrStep = BytesPerPixel[static_cast<size_t>(destPixFmt)];

    for (unsigned j = 0; j < height; j++)
    {
        const uint8_t* inPtr = srcBuf.GetRowAs<uint8_t>(j + y0) + x0 * srcBuf.GetBytesPerPixel();
        uint8_t* outPtr = destBuf.GetRowAs<uint8_t>(j);

        switch (srcBuf.GetPixelFormat())
        {
        case PixelFormat::PIX_MONO8: {
            switch (destPixFmt)
            {
            case PixelFormat::PIX_MONO16:
                ConvertWholeLine<uint8_t, uint16_t>([](const uint8_t* src, uint16_t* dest) {
                    *dest = static_cast<uint16_t>(*src) << 8;
                },
                width, inPtr, inPtrStep, outPtr, outPtrStep);
                break;

            case PixelFormat::PIX_MONO32F:
                ConvertWholeLine<uint8_t, float>([](const uint8_t* src, float* dest) {
                    *dest = *src * 1.0f/0xFF;
                },
                width, inPtr, inPtrStep, outPtr, outPtrStep);
                break;

            case PixelFormat::PIX_RGB8:
                ConvertWholeLine<uint8_t, uint8_t>([](const uint8_t* src, uint8_t* dest) {
                    dest[0] = dest[1] = dest[2] = *src;
                },
                width, inPtr, inPtrStep, outPtr, outPtrStep);
                break;

            case PixelFormat::PIX_RGB16:
                ConvertWholeLine<uint8_t, uint16_t>([](const uint8_t* src, uint16_t* dest) {
                    dest[0] = dest[1] = dest[2] = static_cast<uint16_t>(*src) << 8;
                },
                width, inPtr, inPtrStep, outPtr, outPtrStep);
                break;

            default: IMPPG_ABORT();
            }
        } break;

        case PixelFormat::PIX_MONO16: {
            switch (destPixFmt)
            {
            case PixelFormat::PIX_MONO8: ConvertWholeLine<uint16_t, uint8_t>([](const uint16_t* src, uint8_t* dest) {
                *dest = static_cast<uint8_t>(*src >> 8);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO32F: ConvertWholeLine<uint16_t, float>([](const uint16_t* src, float* dest) {
                *dest = *src * 1.0f/0xFFFF;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB8: ConvertWholeLine<uint16_t, uint8_t>([](const uint16_t* src, uint8_t* dest) {
                dest[0] = dest[1] = dest[2] = static_cast<uint8_t>(*src >> 8);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB16: ConvertWholeLine<uint16_t, uint16_t>([](const uint16_t* src, uint16_t* dest) {
                dest[0] = dest[1] = dest[2] = *src;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            default: IMPPG_ABORT();
            }
        } break;

        case PixelFormat::PIX_MONO32F: {
            switch (destPixFmt)
            {
            case PixelFormat::PIX_MONO8: ConvertWholeLine<float, uint8_t>([](const float* src, uint8_t* dest) {
                *dest = static_cast<uint8_t>(*src * 0xFF);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO16: ConvertWholeLine<float, uint16_t>([](const float* src, uint16_t* dest) {
                *dest = static_cast<uint16_t>(*src * 0xFFFF);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;
            case PixelFormat::PIX_RGB8: ConvertWholeLine<float, uint8_t>([](const float* src, uint8_t* dest) {
                dest[0] = dest[1] = dest[2] = static_cast<uint8_t>(*src * 0xFF);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB16: ConvertWholeLine<float, uint16_t>([](const float* src, uint16_t* dest) {
                dest[0] = dest[1] = dest[2] =  static_cast<uint16_t>(*src * 0xFFFF);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB32F: ConvertWholeLine<float, float>([](const float* src, float* dest) {
                dest[0] = dest[1] = dest[2] = src[0];
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            default: IMPPG_ABORT();
            }
        } break;

        // when converting from a color format to mono, use sum (scaled) of all channels as the pixel brightness
        case PixelFormat::PIX_PAL8: {
            const IImageBuffer::Palette& palette = srcBuf.GetPalette();

            switch (destPixFmt)
            {
            case PixelFormat::PIX_MONO8: ConvertWholeLine<uint8_t, uint8_t>([&palette](const uint8_t* src, uint8_t* dest) {
                *dest = (palette[3*(*src)] + palette[3*(*src)+1] + palette[3*(*src)+2]) / 3;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO16: ConvertWholeLine<uint8_t, uint16_t>([&palette](const uint8_t* src, uint16_t* dest) {
                *dest = (palette[3*(*src)] + palette[3*(*src)+1] + palette[3*(*src)+2]) / 3;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO32F: ConvertWholeLine<uint8_t, float>([&palette](const uint8_t* src, float* dest) {
                *dest = (palette[3*(*src)] + palette[3*(*src)+1] + palette[3*(*src)+2]) * 1.0f / (3*0xFF);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB8: ConvertWholeLine<uint8_t, uint8_t>([&palette](const uint8_t* src, uint8_t* dest) {
                dest[0] = palette[3*(*src)];
                dest[1] = palette[3*(*src)+1];
                dest[2] = palette[3*(*src)+2];
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB16: ConvertWholeLine<uint8_t, uint16_t>([&palette](const uint8_t* src, uint16_t* dest) {
                dest[0] = static_cast<uint16_t>(palette[3*(*src)]) << 8;
                dest[1] = static_cast<uint16_t>(palette[3*(*src)+1]) << 8;
                dest[2] = static_cast<uint16_t>(palette[3*(*src)+2]) << 8;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            default: IMPPG_ABORT();
            }
        } break;

        case PixelFormat::PIX_RGB8:
        case PixelFormat::PIX_RGBA8: {
            switch (destPixFmt)
            {
            case PixelFormat::PIX_MONO8: ConvertWholeLine<uint8_t, uint8_t>([](const uint8_t* src, uint8_t* dest) {
                *dest = (src[0] + src[1] + src[2]) / 3;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO16: ConvertWholeLine<uint8_t, uint16_t>([](const uint8_t* src, uint16_t* dest) {
                *dest = ((src[0] + src[1] + src[2]) << 8) / 3;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO32F: ConvertWholeLine<uint8_t, float>([](const uint8_t* src, float* dest) {
                *dest = (src[0] + src[1] + src[2]) * 1.0f/(3*0xFF);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB32F: ConvertWholeLine<uint8_t, float>([](const uint8_t* src, float* dest) {
                dest[0] = src[0] / static_cast<float>(0xFF);
                dest[1] = src[1] / static_cast<float>(0xFF);
                dest[2] = src[2] / static_cast<float>(0xFF);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB16: ConvertWholeLine<uint8_t, uint16_t>([](const uint8_t* src, uint16_t* dest) {
                dest[0] = src[0] << 8;
                dest[1] = src[1] << 8;
                dest[2] = src[2] << 8;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            default: IMPPG_ABORT();
            }
        } break;

        case PixelFormat::PIX_BGR8:
        case PixelFormat::PIX_BGRA8: {
            switch (destPixFmt)
            {
            case PixelFormat::PIX_RGB8: ConvertWholeLine<uint8_t, uint8_t>([](const uint8_t* src, uint8_t* dest) {
                dest[0] = src[2];
                dest[1] = src[1];
                dest[2] = src[0];
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO8: ConvertWholeLine<uint8_t, uint8_t>([](const uint8_t* src, uint8_t* dest) {
                *dest = (src[0] + src[1] + src[2]) / 3;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO16: ConvertWholeLine<uint8_t, uint16_t>([](const uint8_t* src, uint16_t* dest) {
                *dest = ((src[0] + src[1] + src[2]) << 8) / 3;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO32F: ConvertWholeLine<uint8_t, float>([](const uint8_t* src, float* dest) {
                *dest = (src[0] + src[1] + src[2]) * 1.0f/(3*0xFF);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB32F: ConvertWholeLine<uint8_t, float>([](const uint8_t* src, float* dest) {
                dest[0] = src[2] / static_cast<float>(0xFF);
                dest[1] = src[1] / static_cast<float>(0xFF);
                dest[2] = src[0] / static_cast<float>(0xFF);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB16: ConvertWholeLine<uint8_t, uint16_t>([](const uint8_t* src, uint16_t* dest) {
                dest[0] = src[2] << 8;
                dest[1] = src[1] << 8;
                dest[2] = src[0] << 8;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;


            default: IMPPG_ABORT();
            }
        } break;

        case PixelFormat::PIX_RGB16:
        case PixelFormat::PIX_RGBA16: {
            switch (destPixFmt)
            {
            case PixelFormat::PIX_MONO8: ConvertWholeLine<uint16_t, uint8_t>([](const uint16_t* src, uint8_t* dest) {
                *dest = ((src[0] + src[1] + src[2]) / 3) >> 8;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO16: ConvertWholeLine<uint16_t, uint16_t>([](const uint16_t* src, uint16_t* dest) {
                *dest = (src[0] + src[1] + src[2]) / 3;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO32F: ConvertWholeLine<uint16_t, float>([](const uint16_t* src, float* dest) {
                *dest = (src[0] + src[1] + src[2]) * 1.0f/(3*0xFFFF);
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB8: ConvertWholeLine<uint16_t, uint8_t>([](const uint16_t* src, uint8_t* dest) {
                dest[0] = src[0] >> 8;
                dest[1] = src[1] >> 8;
                dest[2] = src[2] >> 8;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB32F: ConvertWholeLine<std::uint16_t, float>([](const std::uint16_t* src, float* dest) {
                dest[0] = static_cast<float>(src[0]) / 0xFFFF;
                dest[1] = static_cast<float>(src[1]) / 0xFFFF;
                dest[2] = static_cast<float>(src[2]) / 0xFFFF;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            default: IMPPG_ABORT();
            }
        } break;

        case PixelFormat::PIX_RGB32F:
        case PixelFormat::PIX_RGBA32F: {
            switch (destPixFmt)
            {
            case PixelFormat::PIX_MONO8: ConvertWholeLine<float, uint8_t>([](const float* src, uint8_t* dest) {
                *dest = ((src[0] + src[1] + src[2]) / 3) * 0xFF;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO16: ConvertWholeLine<float, uint16_t>([](const float* src, uint16_t* dest) {
                *dest = ((src[0] + src[1] + src[2]) / 3) * 0xFFFF;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_MONO32F: ConvertWholeLine<float, float>([](const float* src, float* dest) {
                *dest = (src[0] + src[1] + src[2]) / 3.0;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB8: ConvertWholeLine<float, uint8_t>([](const float* src, uint8_t* dest) {
                dest[0] = src[0] * 0xFF;
                dest[1] = src[1] * 0xFF;
                dest[2] = src[2] * 0xFF;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            case PixelFormat::PIX_RGB16: ConvertWholeLine<float, uint16_t>([](const float* src, uint16_t* dest) {
                dest[0] = src[0] * 0xFFFF;
                dest[1] = src[1] * 0xFFFF;
                dest[2] = src[2] * 0xFFFF;
            },
            width, inPtr, inPtrStep, outPtr, outPtrStep);
            break;

            default: IMPPG_ABORT();
            }
        } break;

        default: IMPPG_ABORT();
        }
    }

    return destBuf;
}

static c_SimpleBuffer GetConvertedPixelFormatCopy(const IImageBuffer& srcBuf, PixelFormat destPixFmt)
{
    return GetConvertedPixelFormatFragment(srcBuf, destPixFmt, 0, 0, srcBuf.GetWidth(), srcBuf.GetHeight());
}

/** Converts 'srcImage' to 'destPixFmt'; ; result uses c_SimpleBuffer for storage.
    If 'destPixFmt' is the same as source image's pixel format, returns a copy of 'srcImg'. */
c_Image c_Image::ConvertPixelFormat(PixelFormat destPixFmt) const
{
    return GetConvertedPixelFormatSubImage(destPixFmt, 0, 0, GetWidth(), GetHeight());
}

/// Converts the specified fragment of 'srcImage' to 'destPixFmt'; result uses c_SimpleBuffer for storage
c_Image c_Image::GetConvertedPixelFormatSubImage(PixelFormat destPixFmt, unsigned x0, unsigned y0, unsigned width, unsigned height) const
{
    return c_Image(std::make_unique<c_SimpleBuffer>(GetConvertedPixelFormatFragment(*m_Buffer.get(), destPixFmt, x0, y0, width, height)));
}

/// Copies a rectangular area from 'src' to 'dest'. Pixel formats of 'src' and 'dest' have to be the same.
void c_Image::Copy(const c_Image& src, c_Image& dest, unsigned srcX, unsigned srcY, unsigned width, unsigned height, unsigned destX, unsigned destY)
{
    IMPPG_ASSERT(src.GetPixelFormat() == dest.GetPixelFormat());

    if (srcX >= src.GetWidth())
    {
        return;
    }
    if (srcX + width >= src.GetWidth())
    {
        width = src.GetWidth() - srcX;
    }
    if (srcY >= src.GetHeight())
    {
        return;
    }
    if (srcY + height >= src.GetHeight())
    {
        height = src.GetHeight() - srcY;
    }

    if (destX >= dest.GetWidth())
    {
        return;
    }
    if (destX + width >= dest.GetWidth())
    {
        width = dest.GetWidth() - destX;
    }
    if (destY >= dest.GetHeight())
    {
        return;
    }
    if (destY + height >= dest.GetHeight())
    {
        height = dest.GetHeight() - destY;
    }

    int bpp = src.GetBuffer().GetBytesPerPixel();

    for (unsigned y = 0; y < height; y++)
    {
        memcpy(dest.GetRowAs<uint8_t>(destY + y) + destX * bpp,
               src.GetRowAs<uint8_t>(srcY + y) + srcX * bpp,
               width * bpp);
    }
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
    float delta_k = static_cast<float>(f1) - f0;
    float dk = (static_cast<float>(f1) - fm1)*0.5f, dk1 = (static_cast<float>(f2) - f0)*0.5f;

    float a0 = f0, a1 = dk, a2 = 3.0f*delta_k - 2.0f*dk - dk1,
        a3 = static_cast<float>(dk) + dk1 - 2.0f*delta_k;

    return t*(t*(a3*t + a2)+a1)+a0;
}

template<typename Lum_t>
void ResizeAndTranslateImpl(
    const IImageBuffer& srcImg,
    IImageBuffer& destImg,
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
    float yOfsFrac = std::modf(yOfs, &temp); yOfsInt = static_cast<int>(temp);
    float xOfsFrac = std::modf(xOfs, &temp); xOfsInt = static_cast<int>(temp);

    int bytesPP = srcImg.GetBytesPerPixel();

    // start and end (inclusive) coordinates to fill in the output buffer
    int destXstart = (xOfsInt < 0) ? 0 : xOfsInt;
    int destYstart = (yOfsInt < 0) ? 0 : yOfsInt;

    int destXend = std::min(xOfsInt + srcXmax, static_cast<int>(destImg.GetWidth())-1);
    int destYend = std::min(yOfsInt + srcYmax, static_cast<int>(destImg.GetHeight())-1);

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
            memset(destImg.GetRowAs<uint8_t>(y) + (destXend+1)*bytesPP, 0, (destImg.GetWidth() - 1 - destXend) * bytesPP);
        }
    }

    if (xOfsFrac == 0.0f && yOfsFrac == 0.0f)
    {
        // Not a subpixel translation; just copy the pixels line by line
        for (int y = destYstart; y <= destYend; y++)
        {
            memcpy(
                destImg.GetRowAs<uint8_t>(y) + destXstart * bytesPP,
                srcImg.GetRowAs<uint8_t>(y - yOfsInt + srcYmin) + (destXstart - xOfsInt + srcXmin) * bytesPP,
                (destXend - destXstart + 1) * bytesPP
            );
        }
    }
    else
    {
        // Subpixel translation
        IMPPG_ASSERT(srcImg.GetPixelFormat() != PixelFormat::PIX_PAL8);

        // First, copy the 2-border pixels of the target area without change.

        // 2 top and 2 bottom rows
        for (int i = 0; i < 2; i++)
        {
            memcpy(destImg.GetRowAs<uint8_t>(destYstart + i) + destXstart*bytesPP,
                   srcImg.GetRowAs<uint8_t>(destYstart + i - yOfsInt + srcYmin) + (destXstart - xOfsInt + srcXmin) * bytesPP,
                   (destXend - destXstart + 1) * bytesPP);

            memcpy(destImg.GetRowAs<uint8_t>(destYend - i) + destXstart*bytesPP,
                srcImg.GetRowAs<uint8_t>(destYend - i - yOfsInt + srcYmin) + (destXstart - xOfsInt + srcXmin) * bytesPP,
                (destXend - destXstart + 1) * bytesPP);
        }

        // 2 leftmost and 2 rightmost columns
        for (int y = destYstart; y <= destYend; y++)
        {
            // 2 leftmost columns
            memcpy(destImg.GetRowAs<uint8_t>(y) + destXstart*bytesPP,
                   srcImg.GetRowAs<uint8_t>(y - yOfsInt + srcYmin) + (destXstart - xOfsInt + srcXmin)*bytesPP,
                   2 * bytesPP); // copy 2 pixels

            // 2 rightmost columns
            memcpy(destImg.GetRowAs<uint8_t>(y) + (destXend-1)*bytesPP,
                   srcImg.GetRowAs<uint8_t>(y - yOfsInt + srcYmin) + (destXend - 1 - xOfsInt + srcXmin)*bytesPP,
                   2 * bytesPP); // copy 2 pixels
        }

        // Second, perform cubic interpolation inside the target area.

        float maxLum = 0.0f;
        switch (srcImg.GetPixelFormat())
        {
        case PixelFormat::PIX_MONO8:
        case PixelFormat::PIX_RGB8:
        case PixelFormat::PIX_RGBA8:
            maxLum = static_cast<float>(0xFF); break;

        case PixelFormat::PIX_MONO16:
        case PixelFormat::PIX_RGB16:
        case PixelFormat::PIX_RGBA16:
            maxLum = static_cast<float>(0xFFFF); break;

        case PixelFormat::PIX_MONO32F:
        case PixelFormat::PIX_RGB32F:
        case PixelFormat::PIX_RGBA32F:
            maxLum = 1.0f; break;

        default: IMPPG_ABORT();
        }

        int idx = xOfsFrac < 0.0f ? 1 : -1;
        int idy = yOfsFrac < 0.0f ? 1 : -1;

        xOfsFrac = std::fabs(xOfsFrac);
        yOfsFrac = std::fabs(yOfsFrac);

        int numChannels = NumChannels[static_cast<size_t>(srcImg.GetPixelFormat())];

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
                            srcImg.GetRowAs<Lum_t>(srcY)[(srcX - idx)*numChannels + ch],
                            srcImg.GetRowAs<Lum_t>(srcY)[(srcX      )*numChannels + ch],
                            srcImg.GetRowAs<Lum_t>(srcY)[(srcX + idx)*numChannels + ch],
                            srcImg.GetRowAs<Lum_t>(srcY)[(srcX + idx + idx)*numChannels + ch]);

                        y += idy;
                        srcY += idy;
                    }

                    // Perform the final vertical (column) interpolation of the 4 horizontal (row) values interpolated previously
                    destImg.GetRowAs<Lum_t>(row)[col*numChannels + ch] = static_cast<Lum_t>(ClampLuminance(InterpolateCubic(yOfsFrac, yvals[0], yvals[1], yvals[2], yvals[3]), maxLum));
                }
            }
        }
    }
}

/// Resizes and translates image (or its fragment) by cropping and/or padding (with zeros) to the destination size and offset (there is no scaling)
/** Subpixel translation (i.e. if 'xOfs' or 'yOfs' have a fractional part) of palettised images is not supported. */
void c_Image::ResizeAndTranslate(
    const IImageBuffer& srcImg,
    IImageBuffer& destImg,
    int srcXmin,     ///< X min of input data in source image
    int srcYmin,     ///< Y min of input data in source image
    int srcXmax,     ///< X max of input data in source image
    int srcYmax,     ///< Y max of input data in source image
    float xOfs,      ///< X offset of input data in output image
    float yOfs,      ///< Y offset of input data in output image
    bool clearToZero ///< If 'true', 'destImg' areas not copied on will be cleared to zero
    )
{
    IMPPG_ASSERT(srcImg.GetPixelFormat() == destImg.GetPixelFormat());

    switch (srcImg.GetPixelFormat())
    {
    case PixelFormat::PIX_MONO8:
    case PixelFormat::PIX_RGB8:
    case PixelFormat::PIX_RGBA8:
        ResizeAndTranslateImpl<uint8_t>(srcImg, destImg, srcXmin, srcYmin, srcXmax, srcYmax, xOfs, yOfs, clearToZero); break;

    case PixelFormat::PIX_MONO16:
    case PixelFormat::PIX_RGB16:
    case PixelFormat::PIX_RGBA16:
        ResizeAndTranslateImpl<uint16_t>(srcImg, destImg, srcXmin, srcYmin, srcXmax, srcYmax, xOfs, yOfs, clearToZero); break;

    case PixelFormat::PIX_MONO32F:
    case PixelFormat::PIX_RGB32F:
    case PixelFormat::PIX_RGBA32F:
        ResizeAndTranslateImpl<float>(srcImg, destImg, srcXmin, srcYmin, srcXmax, srcYmax, xOfs, yOfs, clearToZero); break;

    default: IMPPG_ABORT();
    }
}

void NormalizeFpImage(c_Image& img, float minLevel, float maxLevel)
{
    IMPPG_ASSERT(
        img.GetPixelFormat() == PixelFormat::PIX_MONO32F ||
        img.GetPixelFormat() == PixelFormat::PIX_RGB32F
    );

    const std::size_t numChannels = NumChannels[static_cast<std::size_t>(img.GetPixelFormat())];

    // min and max brightness in the input image
    float lmin = FLT_MAX;
    float lmax = -FLT_MAX;
    for (unsigned row = 0; row < img.GetHeight(); row++)
        for (unsigned col = 0; col < img.GetWidth() * numChannels; col++)
        {
            const float val = img.GetRowAs<float>(row)[col];
            if (val < lmin)
                lmin = val;
            if (val > lmax)
                lmax = val;
        }

    // Determine coefficients 'a' and 'b' which satisfy: new_luminance := a * old_luminance + b
    const float a = (maxLevel - minLevel) / (lmax - lmin);
    const float b = maxLevel - a * lmax;

    // Pixels with brightness 'minLevel' become black and those of 'maxLevel' become white.

    for (unsigned row = 0; row < img.GetHeight(); row++)
        for (unsigned col = 0; col < img.GetWidth() * numChannels; col++)
        {
            float& val = img.GetRowAs<float>(row)[col];
            val = a * val + b;
            if (val < 0.0f)
                val = 0.0f;
            else if (val > 1.0f)
                val = 1.0f;
        }
}

#if USE_CFITSIO
std::optional<c_Image> LoadFitsImage(const std::string& fname, bool normalize)
{
    fitsfile* fptr{nullptr};
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
            return std::nullopt;
        }
        else
            break;

        //TODO: if 3 axes are detected, convert RGB to grayscale; now we only load the first channel
    }

    if (dimensions[0] < 0 || dimensions[1] < 0)
        return std::nullopt;

    size_t numPixels = dimensions[0] * dimensions[1];
    std::unique_ptr<uint8_t[]> fitsPixels{nullptr};
    int destType; // data type that the pixels will be converted to on read
    PixelFormat pixFmt;

    switch (bitsPerPixel)
    {
    case BYTE_IMG:
        fitsPixels.reset(new uint8_t[numPixels * sizeof(uint8_t)]);
        destType = TBYTE;
        pixFmt = PixelFormat::PIX_MONO8;
        break;

    case SHORT_IMG:
        fitsPixels.reset(new uint8_t[numPixels * sizeof(uint16_t)]);
        destType = TUSHORT;
        pixFmt = PixelFormat::PIX_MONO16;
        break;

    default:
        // all the remaining types will be converted to 32-bit floating-point
        fitsPixels.reset(new uint8_t[numPixels * sizeof(float)]);
        destType = TFLOAT;
        pixFmt = PixelFormat::PIX_MONO32F;
        break;
    }

    fits_read_img(fptr, destType, 1, numPixels, 0, fitsPixels.get(), 0, &status);

    if (NUM_OVERFLOW == status && (bitsPerPixel == BYTE_IMG || bitsPerPixel == SHORT_IMG))
    {
        // Input file had some negative values; let us just load it as floating-point
        status = 0;
        fitsPixels.reset(new uint8_t[numPixels * sizeof(float)]);
        destType = TFLOAT;
        pixFmt = PixelFormat::PIX_MONO32F;
        fits_read_img(fptr, destType, 1, numPixels, 0, fitsPixels.get(), 0, &status);
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
            for (unsigned long y = 0; y < static_cast<unsigned long>(dimensions[1]); y++)
                for (unsigned long  x = 0; x < static_cast<unsigned long>(dimensions[0]); x++)
                {
                    float& val = reinterpret_cast<float*>(fitsPixels.get())[x + y*dimensions[0]];
                    if (val < 0.0f)
                        val = 0.0f;
                    else if (val > maxval)
                        maxval = val;
                }

            if (maxval > 1.0f)
            {
                if (normalize)
                {
                    float maxvalinv = 1.0f/maxval;
                    for (int y = 0; y < dimensions[1]; y++)
                        for (int x = 0; x < dimensions[0]; x++)
                            reinterpret_cast<float*>(fitsPixels.get())[x + y*dimensions[0]] *= maxvalinv;
                }
                else
                {
                    for (int y = 0; y < dimensions[1]; y++)
                    {
                        for (int x = 0; x < dimensions[0]; x++)
                        {
                            float& val = reinterpret_cast<float*>(fitsPixels.get())[x + y*dimensions[0]];
                            if (val > 1.0) val = 1.0;
                        }
                    }
                }
            }
        }

        auto result = c_Image(dimensions[0], dimensions[1], pixFmt);
        const auto bpp = BytesPerPixel[static_cast<size_t>(pixFmt)];
        for (unsigned row = 0; row < result.GetHeight(); row++)
        {
            memcpy(
                result.GetRow(row),
                fitsPixels.get() + row * dimensions[0] * bpp,
                dimensions[0] * bpp
            );
        }

        return result;
    }
    else
        return std::nullopt;
}
#endif

std::optional<c_Image> LoadImage(
    const std::string& fname,
    std::optional<PixelFormat> destFmt, ///< Pixel format to convert to; can be one of PIX_MONO8, PIX_MONO32F.
    std::string* errorMsg, ///< If not null, may receive an error message (if any).
    /// If true, floating-points values read from a FITS file are normalized, so that the highest becomes 1.0.
    bool normalizeFITSvalues
)
{
    if (errorMsg)
        *errorMsg = "";

    const auto extension = GetExtension(fname);

#if USE_CFITSIO
    if (extension == "fit" || extension == "fits")
    {
        std::optional<c_Image> result = LoadFitsImage(fname, normalizeFITSvalues);
        if (result.has_value())
        {
            if (!destFmt.has_value() || result->GetPixelFormat() == destFmt.value())
                return result;
            else
                return result->ConvertPixelFormat(destFmt.value());
        }
        else
            return std::nullopt;
    }
#endif

#if USE_FREEIMAGE
    //TODO: add handling of FreeImage's error message (if any)

    FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
    fif = FreeImage_GetFileType(fname.c_str());
    if (FIF_UNKNOWN == fif)
    {
        fif = FreeImage_GetFIFFromFilename(fname.c_str());
    }

    if (fif == FIF_UNKNOWN || !FreeImage_FIFSupportsReading(fif))
    {
        return std::nullopt;
    }

    c_FreeImageHandleWrapper fibmp(FreeImage_Load(fif, fname.c_str()));
    if (!fibmp) { return std::nullopt; }

    auto buf = c_FreeImageBuffer::Create(std::move(fibmp));
    if (!buf) { return std::nullopt; }

    auto image = c_Image(std::move(buf.value()));

    if (destFmt.has_value())
    {
        image = image.ConvertPixelFormat(destFmt.value());
    }
    else
    {
        // Makes a copy backed by `c_SimpleBuffer`; there had been performance issues with `c_FreeImageBuffer` due to it
        // using FreeImage calls internally (TODO: are there still?).
        const auto currentFmt = image.GetPixelFormat();
        const auto targetFormat = (currentFmt == PixelFormat::PIX_BGR8 || currentFmt == PixelFormat::PIX_BGRA8)
            ? PixelFormat::PIX_RGB8
            : currentFmt;
        image = image.ConvertPixelFormat(targetFormat);
    }

    return image;

#else // if USE_FREEIMAGE

        std::optional<c_Image> newImg;
        if (extension == "tif" || extension == "tiff")
            newImg = ReadTiff(fname.c_str(), errorMsg);
        else if (extension == "bmp")
            newImg = ReadBmp(fname.c_str());

        if (!newImg)
        {
            return std::nullopt;
        }
        else if (destFmt.has_value() && newImg->GetPixelFormat() != destFmt.value())
        {
            return newImg.value().ConvertPixelFormat(destFmt.value());
        }
        else
        {
            return newImg.value();
        }
#endif
}

std::optional<c_Image> LoadImageFileAs32f(
    const std::string& fname,
    bool normalizeFITSvalues,
    std::string* errorMsg
)
{
    const auto image = LoadImage(fname, std::nullopt, errorMsg, normalizeFITSvalues);
    if (!image) { return std::nullopt; }

    if (NumChannels[static_cast<std::size_t>(image->GetPixelFormat())] == 1)
    {
        return image->ConvertPixelFormat(PixelFormat::PIX_MONO32F);
    }
    else
    {
        return image->ConvertPixelFormat(PixelFormat::PIX_RGB32F);
    }
}

std::optional<c_Image> LoadImageFileAsMono32f(
    const std::string& fname,
    bool normalizeFITSvalues,
    std::string* errorMsg
)
{
    return LoadImage(fname, PixelFormat::PIX_MONO32F, errorMsg, normalizeFITSvalues);
}

std::optional<c_Image> LoadImageFileAsMono8(
    const std::string& fname,
    bool normalizeFITSvalues,
    std::string* errorMsg
)
{
    return LoadImage(fname, PixelFormat::PIX_MONO8, errorMsg, normalizeFITSvalues);
}

/// Multiply by another image; both images have to be PIX_MONO32F and have the same dimensions
void c_Image::Multiply(const c_Image& mult)
{
    IMPPG_ASSERT(GetPixelFormat() == PixelFormat::PIX_MONO32F && mult.GetPixelFormat() == PixelFormat::PIX_MONO32F);
    IMPPG_ASSERT(GetWidth() == mult.GetWidth() && GetHeight() == mult.GetHeight());

    for (unsigned row = 0; row < GetHeight(); row++)
    {
        const float* src_row = mult.GetRowAs<float>(row);
        float* dest_row = GetRowAs<float>(row);
        for (unsigned col = 0; col < GetWidth(); col++)
            dest_row[col] *= src_row[col];
    }
}

/// Returns 'true' if image's width and height were successfully read; returns 'false' on error
std::optional<std::tuple<unsigned, unsigned>> GetImageSize(const std::string& fname)
{
    const auto extension = GetExtension(fname);
#if USE_CFITSIO
    if (extension == "fit" || extension == "fits")
    {
        fitsfile* fptr{nullptr};
        int status = 0;
        fits_open_file(&fptr, fname.c_str(), READONLY, &status);
        long dimensions[2] = { 0 }; //TODO: support 3-axis files too
        int naxis = 0;
        fits_read_imghdr(fptr, 2, 0, 0, &naxis, dimensions, 0, 0, 0, &status);
        fits_close_file(fptr, &status);

        if (naxis != 2 || status)
            return std::nullopt;
        else
            return std::make_tuple(dimensions[0], dimensions[1]);
    }
#endif
#if USE_FREEIMAGE
    (void)extension; // avoid unused parameter warning when USE_CFITSIO is 0
    FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
    fif = FreeImage_GetFileType(fname.c_str());
    if (FIF_UNKNOWN == fif)
    {
        fif = FreeImage_GetFIFFromFilename(fname.c_str());
    }

    if (fif != FIF_UNKNOWN && FreeImage_FIFSupportsReading(fif))
    {
        c_FreeImageHandleWrapper fibmp = FreeImage_Load(fif, fname.c_str(), FIF_LOAD_NOPIXELS);
        if (!fibmp)
            return std::nullopt;

        return std::make_tuple(FreeImage_GetWidth(fibmp.get()), FreeImage_GetHeight(fibmp.get()));
    }
    else
        return std::nullopt;
#else
    if (extension == "tif" || extension == "tiff")
        return GetTiffDimensions(fname.c_str());
    else if (extension == "bmp")
        return GetBmpDimensions(fname.c_str());
    else
        return std::nullopt;
#endif
}

static PixelFormat GetOutputPixelFormat(PixelFormat srcFmt, OutputBitDepth outpBitDepth)
{
    if (outpBitDepth == OutputBitDepth::Unchanged)
        return srcFmt;

    switch (srcFmt)
    {
        case PixelFormat::PIX_MONO8:
        case PixelFormat::PIX_MONO16:
        case PixelFormat::PIX_MONO32F:
            switch (outpBitDepth)
            {
            case OutputBitDepth::Uint8: return PixelFormat::PIX_MONO8;
            case OutputBitDepth::Uint16: return PixelFormat::PIX_MONO16;
#if USE_FREEIMAGE || USE_CFITSIO
            case OutputBitDepth::Float32: return PixelFormat::PIX_MONO32F;
#endif
            default: break;
            }
            break;

        case PixelFormat::PIX_RGB8:
        case PixelFormat::PIX_RGB16:
        case PixelFormat::PIX_RGB32F:
            switch (outpBitDepth)
            {
            case OutputBitDepth::Uint8: return PixelFormat::PIX_RGB8;
            case OutputBitDepth::Uint16: return PixelFormat::PIX_RGB16;
#if USE_FREEIMAGE || USE_CFITSIO
            case OutputBitDepth::Float32: return PixelFormat::PIX_RGB32F;
#endif
            default: break;
            }
            break;

        case PixelFormat::PIX_RGBA8:
        case PixelFormat::PIX_RGBA16:
        case PixelFormat::PIX_RGBA32F:
            switch (outpBitDepth)
            {
            case OutputBitDepth::Uint8: return PixelFormat::PIX_RGBA8;
            case OutputBitDepth::Uint16: return PixelFormat::PIX_RGBA16;
#if USE_FREEIMAGE || USE_CFITSIO
            case OutputBitDepth::Float32: return PixelFormat::PIX_RGBA32F;
#endif
            default: break;
            }
            break;

        default: IMPPG_ABORT();
    }

    return srcFmt; // never happens
}

bool c_Image::SaveToFile(const std::string& fname, OutputBitDepth outpBitDepth, OutputFileType outpFileType) const
{
    IMPPG_ASSERT(m_Buffer->GetPixelFormat() != PixelFormat::PIX_PAL8);

    IImageBuffer* bufToSave = m_Buffer.get();
    std::unique_ptr<c_SimpleBuffer> converted;

    const auto destPixFmt = GetOutputPixelFormat(m_Buffer->GetPixelFormat(), outpBitDepth);
    if (m_Buffer->GetPixelFormat() != destPixFmt)
    {
        converted = std::make_unique<c_SimpleBuffer>(GetConvertedPixelFormatCopy(*m_Buffer.get(), destPixFmt));
        bufToSave = converted.get();
    }

    return bufToSave->SaveToFile(fname, outpFileType);
}

static std::tuple<OutputBitDepth, OutputFileType> DecodeOutputFormat(OutputFormat outpFormat)
{
    switch (outpFormat)
    {
    case OutputFormat::BMP_8:        return { OutputBitDepth::Uint8, OutputFileType::BMP };
    case OutputFormat::TIFF_16:      return { OutputBitDepth::Uint16, OutputFileType::TIFF };
#if USE_FREEIMAGE
    case OutputFormat::PNG_8:        return { OutputBitDepth::Uint8, OutputFileType::PNG };
    case OutputFormat::TIFF_8_LZW:   return { OutputBitDepth::Uint8, OutputFileType::TIFF_COMPR_LZW };
    case OutputFormat::TIFF_16_ZIP:  return { OutputBitDepth::Uint16, OutputFileType::TIFF_COMPR_ZIP };
    case OutputFormat::TIFF_32F:     return { OutputBitDepth::Float32, OutputFileType::TIFF };
    case OutputFormat::TIFF_32F_ZIP: return { OutputBitDepth::Float32, OutputFileType::TIFF_COMPR_ZIP };
#endif
#if USE_CFITSIO
    case OutputFormat::FITS_8:       return { OutputBitDepth::Uint8, OutputFileType::FITS };
    case OutputFormat::FITS_16:      return { OutputBitDepth::Uint16, OutputFileType::FITS };
    case OutputFormat::FITS_32F:     return { OutputBitDepth::Float32, OutputFileType::FITS };
#endif

    default: IMPPG_ABORT();
    }
}

bool c_Image::SaveToFile(const std::string& fname, OutputFormat outpFormat) const
{
    IMPPG_ASSERT(m_Buffer->GetPixelFormat() != PixelFormat::PIX_PAL8);

    const auto [outpBitDept, outpFileType] = DecodeOutputFormat(outpFormat);
    return SaveToFile(fname, outpBitDept, outpFileType);
}

std::tuple<c_Image, c_Image, c_Image> c_Image::SplitRGB() const
{
    IMPPG_ASSERT(3 == NumChannels[static_cast<std::size_t>(GetPixelFormat())]);

    const auto destPixFmt = [=]() {
        switch (this->GetPixelFormat())
        {
            case PixelFormat::PIX_RGB8: return PixelFormat::PIX_MONO8;
            case PixelFormat::PIX_RGB16: return PixelFormat::PIX_MONO16;
            case PixelFormat::PIX_RGB32F: return PixelFormat::PIX_MONO32F;

            default: IMPPG_ABORT_MSG("unexpected pixel format");
        }
    }();

    c_Image imgR(GetWidth(), GetHeight(), destPixFmt);
    c_Image imgG(GetWidth(), GetHeight(), destPixFmt);
    c_Image imgB(GetWidth(), GetHeight(), destPixFmt);

    const auto ExtractChannelFromLine = [](unsigned width, std::size_t channel, const auto* src, auto* dest) {
        for (unsigned i = 0; i < width; i++)
        {
            dest[i] = src[3 * i + channel];
        }
    };

    for (unsigned y = 0; y < GetHeight(); ++y)
    {
        switch (destPixFmt)
        {
            case PixelFormat::PIX_MONO8:
                ExtractChannelFromLine(GetWidth(), 0, GetRowAs<std::uint8_t>(y), imgR.GetRowAs<std::uint8_t>(y));
                ExtractChannelFromLine(GetWidth(), 1, GetRowAs<std::uint8_t>(y), imgG.GetRowAs<std::uint8_t>(y));
                ExtractChannelFromLine(GetWidth(), 2, GetRowAs<std::uint8_t>(y), imgB.GetRowAs<std::uint8_t>(y));
                break;

            case PixelFormat::PIX_MONO16:
                ExtractChannelFromLine(GetWidth(), 0, GetRowAs<std::uint16_t>(y), imgR.GetRowAs<std::uint16_t>(y));
                ExtractChannelFromLine(GetWidth(), 1, GetRowAs<std::uint16_t>(y), imgG.GetRowAs<std::uint16_t>(y));
                ExtractChannelFromLine(GetWidth(), 2, GetRowAs<std::uint16_t>(y), imgB.GetRowAs<std::uint16_t>(y));
                break;

            case PixelFormat::PIX_MONO32F:
                ExtractChannelFromLine(GetWidth(), 0, GetRowAs<float>(y), imgR.GetRowAs<float>(y));
                ExtractChannelFromLine(GetWidth(), 1, GetRowAs<float>(y), imgG.GetRowAs<float>(y));
                ExtractChannelFromLine(GetWidth(), 2, GetRowAs<float>(y), imgB.GetRowAs<float>(y));
                break;

            default: IMPPG_ABORT();
        }
    }

    return {imgR, imgG, imgB};
}

c_Image c_Image::CombineRGB(const c_Image& red, const c_Image& green, const c_Image& blue)
{
    for (const auto* img: {&red, &green, &blue})
    {
        IMPPG_ASSERT(1 == NumChannels[static_cast<std::size_t>(img->GetPixelFormat())]);
    }
    IMPPG_ASSERT(red.GetWidth() == green.GetWidth() && red.GetWidth() == blue.GetWidth());
    IMPPG_ASSERT(red.GetHeight() == green.GetHeight() && red.GetHeight() == blue.GetHeight());
    IMPPG_ASSERT(red.GetPixelFormat() == green.GetPixelFormat() && red.GetPixelFormat() == blue.GetPixelFormat());

    const auto destPixFmt = [&]() {
        switch (red.GetPixelFormat())
        {
            case PixelFormat::PIX_MONO8: return PixelFormat::PIX_RGB8;
            case PixelFormat::PIX_MONO16: return PixelFormat::PIX_RGB16;
            case PixelFormat::PIX_MONO32F: return PixelFormat::PIX_RGB32F;

            default: IMPPG_ABORT_MSG("unexpected pixel format");
        }
    }();

    c_Image rgb(red.GetWidth(), red.GetHeight(), destPixFmt);

    const auto CombineChannelsFromLine = [](
        unsigned width,
        const auto* red,
        const auto* green,
        const auto* blue,
        auto* dest
    )
    {
        for (unsigned i = 0; i < width; i++)
        {
            dest[3 * i + 0] = red[i];
            dest[3 * i + 1] = green[i];
            dest[3 * i + 2] = blue[i];
        }
    };

    for (unsigned y = 0; y < red.GetHeight(); ++y)
    {
        switch (destPixFmt)
        {
            case PixelFormat::PIX_RGB8:
                CombineChannelsFromLine(
                    rgb.GetWidth(),
                    red.GetRowAs<std::uint8_t>(y),
                    green.GetRowAs<std::uint8_t>(y),
                    blue.GetRowAs<std::uint8_t>(y),
                    rgb.GetRowAs<std::uint8_t>(y)
                );
                break;

            case PixelFormat::PIX_RGB16:
                CombineChannelsFromLine(
                    rgb.GetWidth(),
                    red.GetRowAs<std::uint16_t>(y),
                    green.GetRowAs<std::uint16_t>(y),
                    blue.GetRowAs<std::uint16_t>(y),
                    rgb.GetRowAs<std::uint16_t>(y)
                );
                break;

            case PixelFormat::PIX_RGB32F:
                CombineChannelsFromLine(
                    rgb.GetWidth(),
                    red.GetRowAs<float>(y),
                    green.GetRowAs<float>(y),
                    blue.GetRowAs<float>(y),
                    rgb.GetRowAs<float>(y)
                );
                break;

            default: IMPPG_ABORT();
        }
    }

    return rgb;
}

c_Image c_Image::Blend(const c_Image& img1, double weight1, const c_Image& img2, double weight2)
{
    IMPPG_ASSERT(weight1 >= 0.0 && weight1 <= 1.0);
    IMPPG_ASSERT(weight2 >= 0.0 && weight2 <= 1.0);
    IMPPG_ASSERT(img1.GetWidth() == img2.GetWidth() && img1.GetHeight() == img2.GetHeight());
    IMPPG_ASSERT(img1.GetPixelFormat() == PixelFormat::PIX_MONO32F ||
                 img1.GetPixelFormat() == PixelFormat::PIX_RGB32F);
    IMPPG_ASSERT(img1.GetPixelFormat() == img2.GetPixelFormat());

    if (weight1 == 0.0 && weight2 == 0.0)
    {
        c_Image black(img1.GetWidth(), img1.GetHeight(), img1.GetPixelFormat());
        black.ClearToZero();
        return black;
    }

    const float actualW1 = weight1 / (weight1 + weight2);
    const float actualW2 = weight2 / (weight1 + weight2);

    c_Image blended(img1.GetWidth(), img1.GetHeight(), img1.GetPixelFormat());

    const auto numChannels = NumChannels[static_cast<int>(img1.GetPixelFormat())];

    for (unsigned y = 0; y < img1.GetHeight(); ++y)
    {
        const float* srcRow1 = img1.GetRowAs<float>(y);
        const float* srcRow2 = img2.GetRowAs<float>(y);
        float* destRow = blended.GetRowAs<float>(y);

        for (unsigned x = 0; x < img1.GetWidth(); ++x)
        {
            for (std::size_t channel = 0; channel < numChannels; ++channel)
            {
                destRow[x * numChannels + channel] =
                    actualW1 * srcRow1[x * numChannels + channel] +
                    actualW2 * srcRow2[x * numChannels + channel];
            }
        }
    }

    return blended;
}

c_Image c_Image::AutomaticWhiteBalance() const
{
    // using the Gray World algorithm

    const unsigned width = GetWidth();
    const unsigned height = GetHeight();

    const auto imgf32 = ConvertPixelFormat(PixelFormat::PIX_RGB32F);

    // do we need these? not much difference when used
    // const auto linear = [](double value) { return pow(value, 2.2); };
    // const auto sRGB = [](double value) { return pow(value, 1/2.2); };
    const auto linear = [](double value) { return value; };
    const auto sRGB = [](double value) { return value; };

    const auto clamp = [](double value) { if (value > 1.0) { return 1.0; } else { return value; } };

    const auto getChannelAvg = [=](const c_Image& img, int channel) {
        IMPPG_ASSERT(img.GetPixelFormat() == PixelFormat::PIX_RGB32F);
        IMPPG_ASSERT(channel >= 0 && channel < 3);

        double sum{0.0};
        for (unsigned y = 0; y < height; ++y)
        {
            const float* line = img.GetBuffer().GetRowAs<float>(y);
            for (unsigned x = 0; x < width; ++x)
            {
                sum += linear(line[3 * x + channel]);
            }
        }

        return sum / (img.GetWidth() * img.GetHeight());
    };

    const double avgR = getChannelAvg(imgf32, 0);
    const double avgG = getChannelAvg(imgf32, 1);
    const double avgB = getChannelAvg(imgf32, 2);

    auto result = c_Image{width, height, PixelFormat::PIX_RGB32F};

    for (unsigned y = 0; y < height; ++y)
    {
        const float* srcLine = imgf32.GetRowAs<float>(y);
        float* destLine = result.GetRowAs<float>(y);

        for (unsigned x = 0; x < width; ++x)
        {
            const auto i = 3 * x;
            destLine[i + 0] = clamp(sRGB(linear(srcLine[i + 0]) * avgG / avgR));
            destLine[i + 1] = clamp(sRGB(linear(srcLine[i + 1])              ));
            destLine[i + 2] = clamp(sRGB(linear(srcLine[i + 2]) * avgG / avgB));
        }
    }

    return result;
}

void c_Image::MultiplyPixelValues(double factor)
{
    IMPPG_ASSERT(GetPixelFormat() == PixelFormat::PIX_MONO32F || GetPixelFormat() == PixelFormat::PIX_RGB32F);
    IMPPG_ASSERT(factor >= 0.0);

    const unsigned width = GetWidth();
    const std::size_t numChannels = NumChannels[static_cast<std::size_t>(GetPixelFormat())];

    for (unsigned y = 0; y < GetHeight(); ++y)
    {
        float* row = GetRowAs<float>(y);
        for (unsigned i = 0; i < width * numChannels; ++i)
        {
            row[i] *= factor;
        }
    }
}
