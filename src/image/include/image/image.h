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
    Image-handling code header.
*/

#ifndef ImPPG_IMAGE_H
#define ImPPG_IMAGE_H

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <wx/gdicmn.h>

#include "common/formats.h"
#include "common/imppg_assert.h"


/// Conditionally swaps a 32-bit value
uint32_t SWAP32cnd(uint32_t x, bool swap);
/// Conditionally swaps the two lower (arithmetically) bytes of a 32-bit value
uint32_t SWAP16in32cnd(uint32_t x, bool swap);
/// Conditionally swaps a 16-bit value
uint16_t SWAP16cnd(uint16_t x, bool swap);

enum class PixelFormat
{
    PIX_PAL8 = 0,  ///< 8-bit with palette (can be a greyscale palette)
    PIX_MONO8,     ///< 8-bit greyscale
    PIX_RGB8,      ///< 24-bit RGB (8 bits per channel)
    PIX_BGR8,      ///< 24-bit BGR (8 bits per channel)
    PIX_RGBA8,     ///< 32-bit RGBA (8 bits per channel)
    PIX_BGRA8,     ///< 32-bit BGRA (8 bits per channel)
    PIX_MONO16,    ///< 16-bit greyscale
    PIX_RGB16,     ///< 48-bit RGB (16 bits per channel)
    PIX_RGBA16,    ///< 64-bit RGBA (16 bits per channel)
    PIX_MONO32F,   ///< 32-bit floating point greyscale
    PIX_RGB32F,    ///< 96-bit floating point RGB
    PIX_RGBA32F,   ///< 128-bit floating point RGBA

    PIX_NUM_FORMATS // this has to be the last element
};

/// Elements correspond to those from PixelFormat
const size_t BytesPerPixel[static_cast<size_t>(PixelFormat::PIX_NUM_FORMATS)] =
{
    1,    // PIX_PAL8
    1,    // PIX_MONO8
    3,    // PIX_RBG8
    3,    // PIX_BGR8
    4,    // PIX_RBGA8
    4,    // PIX_BGRA8
    2,    // PIX_MONO16
    6,    // PIX_RGB16
    8,    // PIX_RGBA16
    4,    // PIX_MONO32F
    12,   // PIX_RGB32F
    16,   // PIX_RGBA32F
};

/// Elements correspond to those from PixelFormat
constexpr size_t NumChannels[static_cast<size_t>(PixelFormat::PIX_NUM_FORMATS)] =
{
    1,    // PIX_PAL8
    1,    // PIX_MONO8
    3,    // PIX_RBG8
    3,    // PIX_BGR8
    4,    // PIX_RBGA8
    4,    // PIX_BGRA8
    1,    // PIX_MONO16
    3,    // PIX_RGB16
    4,    // PIX_RGBA16
    1,    // PIX_MONO32F
    3,    // PIX_RGB32F
    4,    // PIX_RGBA32F
};

constexpr bool IsMono(PixelFormat pixFmt)
{
    return 1 == NumChannels[static_cast<std::size_t>(pixFmt)];
}

class IImageBuffer
{
public:
    static constexpr size_t PALETTE_LENGTH = 256 * 3;
    /// Palette contents (R, G, B) for PIX_PAL8.
    using Palette = std::array<uint8_t, PALETTE_LENGTH>;

    /// Palette is used only if the pixel format is PIX_PAL8.
    virtual Palette& GetPalette() = 0;

    virtual const Palette& GetPalette() const = 0;

    virtual unsigned GetWidth() const = 0;

    virtual unsigned GetHeight() const = 0;

    virtual size_t GetBytesPerRow() const = 0; ///< Returns number of bytes per row (including padding, if any).

    virtual size_t GetBytesPerPixel() const = 0;

    virtual void* GetRow(size_t row) = 0;

    template<typename T>
    T* GetRowAs(size_t row) { return static_cast<T*>(GetRow(row)); }

    virtual const void* GetRow(size_t row) const = 0;

    template<typename T>
    const T* GetRowAs(size_t row) const { return static_cast<const T*>(GetRow(row)); }

    virtual std::unique_ptr<IImageBuffer> GetCopy() const = 0;

    virtual PixelFormat GetPixelFormat() const = 0;

    virtual ~IImageBuffer() = default;

private:
    virtual bool SaveToFile(const std::string& fname, OutputFileType outFileType) const = 0;

    friend class c_Image;
};

class c_Image
{
private:
    std::unique_ptr<IImageBuffer> m_Buffer;

public:
    c_Image(unsigned width, unsigned height, PixelFormat pixFmt);

    c_Image(std::unique_ptr<IImageBuffer> buffer): m_Buffer(std::move(buffer)) {}

    c_Image(c_Image&& img) = default;
    c_Image& operator=(c_Image &&img) = default;

    c_Image(const c_Image &img);
    c_Image& operator=(const c_Image &img);

    void ClearToZero(); ///< Clears all pixels to zero value.

    wxRect GetImageRect() const { return wxRect{ 0, 0, static_cast<int>(GetWidth()), static_cast<int>(GetHeight()) }; };

    unsigned GetWidth() const { return m_Buffer->GetWidth(); }
    unsigned GetHeight() const { return m_Buffer->GetHeight(); }
    unsigned GetNumPixels() const { return GetWidth() * GetHeight(); }
    PixelFormat GetPixelFormat() const { return m_Buffer->GetPixelFormat(); }

    void* GetRow(size_t row) { return m_Buffer->GetRow(row); }
    const void* GetRow(size_t row) const { return m_Buffer->GetRow(row); }

    template <typename T>
    T* GetRowAs(size_t row) { return static_cast<T*>(GetRow(row)); }

    template <typename T>
    const T* GetRowAs(size_t row) const { return static_cast<const T*>(GetRow(row)); }

    IImageBuffer& GetBuffer() { return *m_Buffer; }
    const IImageBuffer& GetBuffer() const { return *m_Buffer; }

    /// Copies a rectangular area from 'src' to 'dest'. Pixel formats of 'src' and 'dest' have to be the same.
    static void Copy(
        const c_Image& src,
        c_Image& dest,
        unsigned srcX,
        unsigned srcY,
        unsigned width,
        unsigned height,
        unsigned destX,
        unsigned destY
    );

    /// Returns image copy converted to `destPixFmt`; result uses `c_SimpleBuffer` for storage.
    c_Image ConvertPixelFormat(PixelFormat destPixFmt) const;

    /// Returns copy of image fragment converted to `destPixFmt`; result uses `c_SimpleBuffer` for storage.
    c_Image GetConvertedPixelFormatSubImage(
        PixelFormat destPixFmt,
        unsigned x0,
        unsigned y0,
        unsigned width,
        unsigned height
    ) const;

    /// Resizes and translates image (or its fragment) by cropping and/or padding (with zeros) to the destination size and offset (there is no scaling).
    /** Subpixel translation (i.e. if `xOfs` or `yOfs` have a fractional part) of palettised images is not supported. */
    static void ResizeAndTranslate(
        const IImageBuffer& srcImg,
        IImageBuffer& destImg,
        int srcXmin,     ///< X min of input data in source image
        int srcYmin,     ///< Y min of input data in source image
        int srcXmax,     ///< X max of input data in source image
        int srcYmax,     ///< Y max of input data in source image
        float xOfs,      ///< X offset of input data in output image
        float yOfs,      ///< Y offset of input data in output image
        bool clearToZero ///< If 'true', 'destImg' areas not copied on will be cleared to zero
        );

    /// Multiply by another image; both images have to be PIX_MONO32F and have the same dimensions
    void Multiply(const c_Image& mult);

    bool SaveToFile(
        const std::string& fname, ///< Full destination path
        OutputBitDepth outpBitDepth,
        OutputFileType outpFileType
    ) const;

    bool SaveToFile(
        const std::string& fname, ///< Full destination path
        OutputFormat outpFormat
    ) const;

    /// Splits RGB image into mono images (same bit depth as original) representing each channel.
    std::tuple<c_Image, c_Image, c_Image> SplitRGB() const;

    static c_Image CombineRGB(const c_Image& red, const c_Image& green, const c_Image& blue);

    //TESTING ####
    static c_Image Blend(const c_Image& img1, double weight1, const c_Image& img2, double weight2);

    c_Image AutomaticWhiteBalance() const;

    void MultiplyPixelValues(double factor);
};

/// Lightweight wrapper of a fragment of an image buffer; does not allocate any pixels memory itself.
///
/// Buf: `IImageBuffer` or `const IImageBuffer`.
///
template<typename Buf>
class c_View
{
    Buf* m_Buf; // must be a pointer rather than a reference to enable move assignment
    int m_X0, m_Y0, m_Width, m_Height;

public:
    c_View(Buf& buf)
        : m_Buf(&buf), m_X0(0), m_Y0(0), m_Width(buf.GetWidth()), m_Height(buf.GetHeight())
    { }

    c_View(Buf& buf, int x0, int y0, int width, int height)
        : m_Buf(&buf), m_X0(x0), m_Y0(y0), m_Width(width), m_Height(height)
    { }

    c_View(Buf& buf, const wxRect& rect)
        : m_Buf(&buf), m_X0(rect.x), m_Y0(rect.y), m_Width(rect.width), m_Height(rect.height)
    { }

    unsigned GetWidth() const { return m_Width; }

    unsigned GetHeight() const { return m_Height; }

    /// Returns number of bytes per row (including padding, if any).
    size_t GetBytesPerRow() const { return m_Buf->GetBytesPerRow(); }

    size_t GetBytesPerPixel() const { return m_Buf->GetBytesPerPixel(); }

    typename std::conditional<std::is_const<Buf>::value, const void*, void*>::type GetRow(size_t row)
    {
        return static_cast<typename std::conditional<std::is_const<Buf>::value, const uint8_t*, uint8_t*>::type>(
            m_Buf->GetRow(m_Y0 + row)) + m_X0 * m_Buf->GetBytesPerPixel();
    }

    /// Returns specified row as a pointer-to-`T`. Constness of `T` has to match constness of `Buf`.
    template <typename T>
    T* GetRowAs(size_t row) { return static_cast<T*>(GetRow(row)); }

    PixelFormat GetPixelFormat() const { return m_Buf->GetPixelFormat(); }
};

#if USE_FREEIMAGE

struct FIBITMAP; // provided by FreeImage.h

class c_FreeImageHandleWrapper
{
    FIBITMAP* m_FiBmp{nullptr};

public:
    c_FreeImageHandleWrapper() = default;

    c_FreeImageHandleWrapper(FIBITMAP* ptr)
    {
        m_FiBmp = ptr;
    }

    c_FreeImageHandleWrapper(const c_FreeImageHandleWrapper& rhs) = delete;

    c_FreeImageHandleWrapper& operator=(const c_FreeImageHandleWrapper& rhs) = delete;

    c_FreeImageHandleWrapper(c_FreeImageHandleWrapper&& rhs) noexcept;

    c_FreeImageHandleWrapper& operator=(c_FreeImageHandleWrapper&& rhs) noexcept;

    ~c_FreeImageHandleWrapper();

    FIBITMAP* get() const { return m_FiBmp; }

    void reset(FIBITMAP* ptr);

    operator bool() const { return m_FiBmp != nullptr; }
};

/// A wrapper around a FreeImage bitmap object.
/** Palettised bitmaps are not supported. Before using c_FreeImageBuffer, convert the wrapped FI bitmap to RGB8. */
class c_FreeImageBuffer: public IImageBuffer
{
    c_FreeImageHandleWrapper m_FiBmp;
    // Store the pointer to the beginning of pixel data and the stride
    // and later calculate row pointers in GetRow(), because for some reason
    // using FreeImage_GetScanLine() is very slow (as of FreeImage 3.16.0,
    // on Windows 8 x64 and Fedora 21 x64).
    uint8_t* m_Pixels;
    PixelFormat m_PixFmt;
    int m_Stride;
    Palette m_Palette{};

    c_FreeImageBuffer(
        c_FreeImageHandleWrapper&& fiBmp,
        uint8_t* pixels,
        PixelFormat pixFmt,
        int stride
    ): m_FiBmp(std::move(fiBmp)), m_Pixels(pixels), m_PixFmt(pixFmt), m_Stride(stride) {}

public:

    c_FreeImageBuffer(const c_FreeImageBuffer& rhs) = delete;
    c_FreeImageBuffer& operator=(const c_FreeImageBuffer& rhs) = delete;

    c_FreeImageBuffer(c_FreeImageBuffer&& rhs) = default;
    c_FreeImageBuffer& operator=(c_FreeImageBuffer&& rhs) = default;

    static std::optional<std::unique_ptr<IImageBuffer>> Create(c_FreeImageHandleWrapper&& fiBmp);

    unsigned GetWidth() const override;

    unsigned GetHeight() const override;

    size_t GetBytesPerRow() const override;

    size_t GetBytesPerPixel() const override;

    void* GetRow(size_t row) override { return m_Pixels + (GetHeight() - 1 - row) * m_Stride; /* freeImage stores rows in reverse order */ }

    const void* GetRow(size_t row) const override { return m_Pixels + (GetHeight() - 1 - row) * m_Stride; }

    PixelFormat GetPixelFormat() const override { return m_PixFmt; }

    Palette& GetPalette() override { return m_Palette; }

    const Palette& GetPalette() const override { return m_Palette; }

    /// Creating a copy is not supported.
    std::unique_ptr<IImageBuffer> GetCopy() const override { IMPPG_ABORT(); }

private:
    bool SaveToFile(const std::string& fname, OutputFileType outFileType) const override;
};
#endif // USE_FREEIMAGE

void NormalizeFpImage(c_Image& img, float minLevel, float maxLevel);

/// Loads image and converts it to PIX_MONO32F or PIX_RGB32F.
std::optional<c_Image> LoadImageFileAs32f(
    const std::string& fname,
    bool normalizeFITSvalues,
    std::string* errorMsg = nullptr  ///< If not null, may receive an error message (if any).
);

/// Loads the specified image file and converts it to PIX_MONO32F.
std::optional<c_Image> LoadImageFileAsMono32f(
    const std::string& fname,        ///< Full path (including file name and extension)
    bool normalizeFITSvalues,
    std::string* errorMsg = nullptr  ///< If not null, may receive an error message (if any)
);

/// Loads the specified image file and converts it to PIX_MONO8.
std::optional<c_Image> LoadImageFileAsMono8(
    const std::string& fname,       ///< Full path (including file name and extension)
    bool normalizeFITSvalues,
    std::string* errorMsg = nullptr ///< If not null, may receive an error message (if any)
);

std::optional<c_Image> LoadImage(
    /// Full path (including file name and extension).
    const std::string& fname,
    /// Pixel format to convert to; can be one of PIX_MONO8, PIX_MONO32F.
    std::optional<PixelFormat> destFmt = std::nullopt,
    ///If not null, may receive an error message (if any).
    std::string* errorMsg = nullptr,
    /// If true, floating-points values read from a FITS file are normalized, so that the highest becomes 1.0.
    bool normalizeFITSvalues = false
);

#if USE_CFITSIO
/// Loads an image from a FITS file; the result's pixel format will be PIX_MONO8, PIX_MONO16 or PIX_MONO32F.
std::optional<c_Image> LoadFitsImage(
    const std::string& fname,
    bool normalize ///< If true, floating-point pixel values will be normalized so that the highest value becomes 1.0.
);
#endif

/// Returns (width, height).
std::optional<std::tuple<unsigned, unsigned>> GetImageSize(
    const std::string& fname     ///< Full path (including file name and extension).
);

#endif // ImPPG_HEADER_H
