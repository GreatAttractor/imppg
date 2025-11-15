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
    Image alignment via phase correlation implementation.
*/

#include <algorithm>
#include <cmath>
#include <complex>
#include <functional>
#include <optional>
#include <wx/arrstr.h>
#include <wx/string.h>
#include <wx/filename.h>

#include "align_common.h"
#include "align_phasecorr.h"
#include "common/common.h"
#include "fft.h"
#include "image/image.h"
#include "common/imppg_assert.h"
#include "logging/logging.h"
#include "math_utils/math_utils.h"

/// Returns 0 for x=0, 1 for x=1
inline float BlackmanWindow(float x)
{
    const float A0 = 7938.0f/18608,
                A1 = 9240.0f/18608,
                A2 = 1430.0f/18608;

    return A0 - A1 * cosf(3.1415926535f*x) + A2 * cosf(2*3.1415926535f*x);
}

/// Calculates window function (Blackman) and returns its values as a PIX_MONO32F image
c_Image CalcWindowFunction(
        int wndWidth,
        int wndHeight
)
{
    c_Image result(wndWidth, wndHeight, PixelFormat::PIX_MONO32F);

    // The window function is horizontally and vertically symmetrical,
    // so calculate it only in a quarter of 'buf'
    for (int y = 0; y < wndHeight/2; y++)
        for (int x = 0; x < wndWidth/2; x++)
        {
            float value = 0.0f;
            float dist = sqr((x - wndWidth*0.5f) / (wndWidth*0.5f)) + sqr((y - wndHeight*0.5f) / (wndHeight*0.5f));
            if (dist < 1)
                value = BlackmanWindow(1.0f - dist);

            // upper left
            result.GetRowAs<float>(y)[x] = value;
            // upper right
            result.GetRowAs<float>(y)[wndWidth-1-x] = value;
            // lower right
            result.GetRowAs<float>(wndHeight-1-y)[wndWidth-1-x] = value;
            // lower left
            result.GetRowAs<float>(wndHeight-1-y)[x] = value;
        }

    return result;
}

/// Determines (using phase correlation) the vector by which image 2 (given by its discrete Fourier transform 'img2FFT') is translated w.r.t. image 1 ('img1FFT')
FloatPoint_t DetermineImageTranslation(
    unsigned Nwidth,  ///< FFT columns
    unsigned Nheight, ///< FFT rows
    const std::complex<float>* img1FFT, ///< FFT of the first image (Nwidth*Nheight elements)
    const std::complex<float>* img2FFT, ///< FFT of the second image (Nwidth*Nheight elements)
    bool subpixelAccuracy ///< If 'true', the translation is determined down to sub-pixel accuracy
)
{
/*
    NOTE: If we do not want sub-pixel accuracy, we want this function to return an integer translation vector (i.e. no fractional part in X or Y).
          Otherwise the caller, DetermineTranslationVectors(), would accumulate the fractional parts in the translation vectors array, but since
          we wouldn't use sub-pixel positioning of output images, there would be a ragged 1-pixel jittering in the output sequence.

          In other words, we must either: return fractional vectors from here and use sub-pixel positioning of output images,
          or: return integer vectors from here and do only integer translations of output images.

          Using (accumulating) fractional vectors and then performing integer-only image translations gives poor results.
 */

    // Using 'operator new[]' (raw memory allocation) instead of new[] to avoid Nwidth*Nheight std::complex constructor calls. All the elements will be assigned to before use.

    // Cross-power spectrum
    std::unique_ptr<std::complex<float>, BlockDeleter> cps(
        static_cast<std::complex<float>*>(operator new[](Nwidth * Nheight * sizeof(std::complex<float>)))
    );

    // Cross-correlation
    std::unique_ptr<std::complex<float>, BlockDeleter> cc(
        static_cast<std::complex<float>*>(operator new[](Nwidth * Nheight * sizeof(std::complex<float>)))
    );

    CalcCrossPowerSpectrum2D(img1FFT, img2FFT, cps.get(), Nwidth * Nheight);
    CalcFFTinv2D(cps.get(), Nheight, Nwidth, cc.get());

    // Find the highest-Re element in cross-correlation array
    unsigned maxx = 0, maxy = 0;
    float maxval = 0.0f;
    for (unsigned y = 0; y < Nheight; y++)
        for (unsigned x = 0; x < Nwidth; x++)
        {
            float currval = cc.get()[x + y*Nwidth].real();
            if (currval > maxval)
            {
                maxval = currval;
                maxx = x;
                maxy = y;
            }
        }

    // Infer the translation vector from the 'cc's largest element's indices

    int Tx, Ty; // prev->curr translation vector
    if (maxx < Nwidth/2)
        Tx = maxx;
    else
        Tx = maxx-Nwidth;

    if (maxy < Nheight/2)
        Ty = maxy;
    else
        Ty = maxy-Nheight;

    float subdx = 0.0f, subdy = 0.0f; // subpixel translation vector
    if (subpixelAccuracy)
    {
        //   Subpixel translation detection based on:
        //
        //     "Extension of Phase Correlation to Subpixel Registration"
        //     Hassan Foroosh, Josiane B. Zerubia, Marc Berthod
        //
        #define CLAMPW(k) (((k)+Nwidth)%Nwidth)
        #define CLAMPH(k) (((k)+Nheight)%Nheight)

        const float ccXhi = cc.get()[CLAMPW(maxx+1) + maxy*Nwidth].real();
        const float ccXlo = cc.get()[CLAMPW(maxx-1) + maxy*Nwidth].real();
        const float ccYhi = cc.get()[maxx + CLAMPH(maxy+1)*Nwidth].real();
        const float ccYlo = cc.get()[maxx + CLAMPH(maxy-1)*Nwidth].real();
        const float ccPeak = cc.get()[maxx + maxy*Nwidth].real();

        if (ccXhi > ccXlo)
        {
            const float dx1 = ccXhi/(ccXhi + ccPeak);
            const float dx2 = ccXhi/(ccXhi - ccPeak);

            if (dx1 > 0 && dx1 < 1.0f)
                subdx = dx1;
            else if (dx2 > 0 && dx2 < 1.0f)
                subdx = dx2;
        }
        else
        {
            const float dx1 = ccXlo/(ccXlo + ccPeak);
            const float dx2 = ccXlo/(ccXlo - ccPeak);

            if (dx1 > 0 && dx1 < 1.0f)
                subdx = -dx1;
            else if (dx2 > 0 && dx2 < 1.0f)
                subdx = -dx2;
        }

        if (ccYhi > ccYlo)
        {
            const float dy1 = ccYhi/(ccYhi + ccPeak);
            const float dy2 = ccYhi/(ccYhi - ccPeak);

            if (dy1 > 0 && dy1 < 1.0f)
                subdy = dy1;
            else if (dy2 > 0 && dy2 < 1.0f)
                subdy = dy2;
        }
        else
        {
            const float dy1 = ccYlo/(ccYlo + ccPeak);
            const float dy2 = ccYlo/(ccYlo - ccPeak);

            if (dy1 > 0 && dy1 < 1.0f)
                subdy = -dy1;
            else if (dy2 > 0 && dy2 < 1.0f)
                subdy = -dy2;
        }
    }

    return FloatPoint_t(Tx + subdx, Ty + subdy);
}

/// Determines translation vector between specified images; the images have to be already multiplied by window function
FloatPoint_t DetermineTranslationVector(
    const c_Image& img1, ///< Width and height have to be the same as 'img2' and be powers of two
    const c_Image& img2  ///< Width and height have to be the same as 'img1' and be powers of two
)
{
    // For details of this function's operation see comments in 'DetermineTranslationVectors()'

    IMPPG_ASSERT(img1.GetWidth() == img2.GetWidth());
    IMPPG_ASSERT(img1.GetHeight() == img2.GetHeight());

    const int width = img1.GetWidth();
    const int height = img1.GetHeight();

    std::unique_ptr<std::complex<float>, BlockDeleter> fft1(
        static_cast<std::complex<float>*>(operator new[](width * height * sizeof(std::complex<float>)))
    );

    std::unique_ptr<std::complex<float>, BlockDeleter> fft2(
        static_cast<std::complex<float>*>(operator new[](width * height * sizeof(std::complex<float>)))
    );

    CalcFFT2D(img1.GetRowAs<float>(0), height, width, img1.GetBuffer().GetBytesPerRow(), fft1.get());
    CalcFFT2D(img2.GetRowAs<float>(0), height, width, img2.GetBuffer().GetBytesPerRow(), fft2.get());

    return DetermineImageTranslation(width, height, fft1.get(), fft2.get(), true);
}

/// Determines translation vectors of an image sequence
bool DetermineTranslationVectors(
        unsigned Nwidth, ///< FFT width
        unsigned Nheight, ///< FFT height
        const AlignmentInputs& inputFiles,
        /// Receives list of translation vectors between files in 'inputFiles'; each vector is a translation relative to the first image
        std::vector<FloatPoint_t>& translation,
        /// Receives the bounding box (within the Nwidth x Nheight working area) of all images after alignment
        Rectangle_t& bBox,
        // (Note: an untranslated image starts in the working area at (Nwidth - imgWidth)/2, (Nheight - imgHeight)/2)
        std::string* errorMsg, ///< If not null, receives error message (if any)
        /// Called after determining an image's translation; argument: index of the current image
        bool subpixelAlignment,
        std::function<void (int, float, float)> progressCallback, ///< Called after determining translation of an image; arguments: image index, trans. vector
        std::function<bool ()> checkAbort, ///< Called periodically to check if there was an "abort processing" request
        bool normalizeFitsValues
)
{
    bool result = true;

    int imgWidth, imgHeight; // Dimensions of the recently read image (before padding to Nwidth*Nheight)

    c_Image windowFunc = CalcWindowFunction(Nwidth, Nheight);
    // Window function smoothly varies from 0 at the array boundaries to 1 at the center and is used to
    // "blunt" the image, starting from the edges. Without it they would produce prominent
    // false peaks in the cross-correlation (as any sudden change in brightness generates
    // lots if high frequencies after FFT), making it very hard or impossible to detect
    // the true peak which corresponds to the actual image translation.

    std::unique_ptr<c_Image> prevImg(new c_Image(Nwidth, Nheight, PixelFormat::PIX_MONO32F)); // previous image in the sequence (padded to Nwidth*Nheight pixels and with window func. applied)
    std::unique_ptr<c_Image> currImg(new c_Image(Nwidth, Nheight, PixelFormat::PIX_MONO32F)); // current image in the sequence (padded to Nwidth*Nheight pixels and with window func. applied)

    // Use 'operator new[]' instead of new[] to avoid Nwidth*Nheight std::complex constructor calls. All the elements will be assigned to before use.

    std::unique_ptr<std::complex<float>, BlockDeleter> prevFFT(
        static_cast<std::complex<float>*>(operator new[](Nwidth * Nheight * sizeof(std::complex<float>)))
    );

    std::unique_ptr<std::complex<float>, BlockDeleter> currFFT(
        static_cast<std::complex<float>*>(operator new[](Nwidth * Nheight * sizeof(std::complex<float>)))
    );

    const auto loadFileByIndex = [&](const wxArrayString& fnames, std::size_t idx) -> std::optional<c_Image> {
        IMPPG_ASSERT(fnames.Count() > idx);
        Log::Print(wxString::Format("Loading %s... ", fnames[idx]));
        std::string localErrorMsg;
        const auto loadResult = LoadImageFileAsMono32f(
            ToFsPath(fnames[idx]),
            normalizeFitsValues,
            &localErrorMsg
        );
        Log::Print("done.\n");
        if (!loadResult)
        {
            if (errorMsg)
            {
                *errorMsg = localErrorMsg;
            }
        }
        return loadResult;
    };

    std::optional<ImageAccessor> src = std::visit(Overload{
        [&](const wxArrayString& fnames) -> std::optional<ImageAccessor> {
            auto loadResult = loadFileByIndex(fnames, 0);
            if (!loadResult.has_value())
            {
                return std::nullopt;
            }
            else
            {
                return ImageAccessor{std::move(*loadResult)};
            }
        },

        [](const InputImageList& images) -> std::optional<ImageAccessor> { return ImageAccessor{images.at(0).get()}; },
    }, inputFiles);

    if (!src.has_value()) { return false; }

    imgWidth = src->Get()->GetWidth();
    imgHeight = src->Get()->GetHeight();

    c_Image::ResizeAndTranslate(src->Get()->GetBuffer(), prevImg->GetBuffer(),
            0, 0, src->Get()->GetWidth()-1, src->Get()->GetHeight()-1,
            (Nwidth - src->Get()->GetWidth())/2, (Nheight - src->Get()->GetHeight())/2, true);

    prevImg->Multiply(windowFunc);

    Log::Print("Calculating FFT... ");
    CalcFFT2D(prevImg->GetRowAs<float>(0), prevImg->GetHeight(), prevImg->GetWidth(), prevImg->GetBuffer().GetBytesPerRow(), prevFFT.get());
    Log::Print("done.");

    // Iterate over the remaining images and detect their translation

    translation.clear();
    translation.push_back(FloatPoint_t(0.0f, 0.0f)); // first element corresponds to the first image - no translation

    // Bounding box of all the images after alignment (in coordinates
    // of the Nwidth x Nheight working buffer, where an untranslated image
    // starts at (Nwidth - imgWidth)/2, (Nheight - imgHeight)/2).
    //
    // Initially corresponds to dimensions and position of the first image.
    bBox.x = (Nwidth - imgWidth)/2;
    bBox.y = (Nheight - imgHeight)/2;

    int xmax = bBox.x + imgWidth - 1;
    int ymax = bBox.y + imgHeight - 1;

    const std::size_t numImages = std::visit(Overload{
        [](const wxArrayString& fnames) { return fnames.Count(); },
        [](const InputImageList& images) { return images.size(); }
    }, inputFiles);

    const auto getFileByIdx = [&](const AlignmentInputs& inputs, std::size_t idx) {
        return std::visit(Overload{
            [&](const wxArrayString& fnames) -> std::optional<ImageAccessor> {
                auto loadResult = loadFileByIndex(fnames, idx);
                if (!loadResult.has_value())
                {
                    return std::nullopt;
                }
                else
                {
                    return ImageAccessor{std::move(*loadResult)};
                }
            },

            [&](const InputImageList& images) -> std::optional<ImageAccessor> { return ImageAccessor{images[idx].get()}; }
        }, inputs);
    };

    for (std::size_t i = 1; i < numImages; ++i)
    {
        const auto src = getFileByIdx(inputFiles, i);
        if (!src.has_value()) { return false; }

        imgWidth = src->Get()->GetWidth();
        imgHeight = src->Get()->GetHeight();

        c_Image::ResizeAndTranslate(src->Get()->GetBuffer(), currImg->GetBuffer(),
                0, 0, src->Get()->GetWidth()-1, src->Get()->GetHeight()-1,
                (Nwidth - src->Get()->GetWidth())/2, (Nheight - src->Get()->GetHeight())/2, true);

        currImg->Multiply(windowFunc);

        // Calculate the current image's FFT
        Log::Print("Calculating FFT... ");
        CalcFFT2D(currImg->GetRowAs<float>(0), currImg->GetHeight(), currImg->GetWidth(), currImg->GetBuffer().GetBytesPerRow(), currFFT.get());
        Log::Print("done.\n");

        FloatPoint_t T = DetermineImageTranslation(Nwidth, Nheight, prevFFT.get(), currFFT.get(), subpixelAlignment);

        FloatPoint_t Tprev = translation.back();
        translation.push_back(FloatPoint_t(Tprev.x + T.x, Tprev.y + T.y));

        float intTx, intTy;
        std::modf(translation.back().x, &intTx);
        std::modf(translation.back().y, &intTy);

        bBox.x = std::min(bBox.x, static_cast<int>(Nwidth - imgWidth)/2 - static_cast<int>(intTx));
        bBox.y = std::min(bBox.y, static_cast<int>(Nheight - imgHeight)/2 - static_cast<int>(intTy));

        int newXmax = (Nwidth - imgWidth)/2 - static_cast<int>(intTx) + imgWidth - 1;
        int newYmax = (Nheight - imgHeight)/2 - static_cast<int>(intTy) + imgHeight - 1;

        if (newXmax > xmax) xmax = newXmax;
        if (newYmax > ymax) ymax = newYmax;

        // Swap pointers for the next iteration
        currImg.swap(prevImg);
        prevFFT.swap(currFFT);

        progressCallback(i, translation.back().x, translation.back().y);
        if (checkAbort())
        {
            result = false;
            break;
        }
    }

    bBox.width = xmax - bBox.x + 1;
    bBox.height = ymax - bBox.y + 1;

    return result;
}

/// Returns the smallest power of 2 which is > n
unsigned GetClosestGPowerOf2(unsigned n)
{
    int msb = 0;
    while (n != 0)
    {
        n >>= 1;
        msb++;
    }

    return (1U << msb);
}

/// Returns the set-theoretic intersection, i.e. the largest shared area, of specified images
Rectangle_t DetermineImageIntersection(
        unsigned Nwidth,    ///< Width of the working buffer (i.e. FFT arrays)
        unsigned Nheight,   ///< Height of the working buffer (i.e. FFT arrays)
        const std::vector<FloatPoint_t>& translation, ///< Translation vectors relative to the first image
        const std::vector<Point_t>& imgSize           ///< Image sizes
)
{
    // Image intersection to be returned. Coordinates are relative to the NxN working buffer,
    // where an untranslated image starts at (N-imgWidth)/2, (N-imgHeight)/2.
    Rectangle_t result;

    // Set the intersection to cover the first image
    result.x = (Nwidth - imgSize[0].x)/2;
    result.y = (Nheight - imgSize[0].y)/2;

    int xmax = result.x + imgSize[0].x - 1;
    int ymax = result.y + imgSize[0].y - 1;

    for (unsigned i = 1; i < translation.size(); i++)
    {
        float intTx, intTy;
        std::modf(translation[i].x, &intTx);
        std::modf(translation[i].y, &intTy);

        result.x = std::max(result.x, static_cast<int>(Nwidth - imgSize[i].x)/2 - static_cast<int>(intTx));
        result.y = std::max(result.y, static_cast<int>(Nheight - imgSize[i].y)/2 - static_cast<int>(intTy));

        int newXmax = (Nwidth - imgSize[i].x)/2 - static_cast<int>(intTx) + imgSize[i].x - 1;
        int newYmax = (Nheight - imgSize[i].y)/2 - static_cast<int>(intTy) + imgSize[i].y - 1;

        if (newXmax < xmax) xmax = newXmax;
        if (newYmax < ymax) ymax = newYmax;
    }

    result.width = xmax - result.x + 1;
    result.height = ymax - result.y + 1;

    return result;
}
