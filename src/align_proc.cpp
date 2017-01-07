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
    Image alignment worker thread implementation.
*/

#include <climits>
#include <cmath>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <boost/math/special_functions/round.hpp>
#include <wx/filename.h>
#include "logging.h"
#include "align_proc.h"
#include "align_phasecorr.h"
#include "common.h"
#include "image.h"
#include "align_disc.h"
#include "gauss.h"
#include "lrdeconv.h"
#if USE_FREEIMAGE
#include "FreeImage.h"
#else
#include "tiff.h"
#include "bmp.h"
#endif

#if USE_FREEIMAGE

/// Wrapper for a FreeImage bitmap handle. Makes sure the bitmap is always unloaded.
class c_ScopedFreeImageBmp
{
    FIBITMAP *m_FiBmp;
public:
    c_ScopedFreeImageBmp(FIBITMAP *fibmp = 0) : m_FiBmp(fibmp) { }
    ~c_ScopedFreeImageBmp()
    {
        if (m_FiBmp)
            FreeImage_Unload(m_FiBmp);
    }

    FIBITMAP *Get() { return m_FiBmp; }
    void Reset(FIBITMAP *fibmp = 0)
    {
        if (m_FiBmp)
            FreeImage_Unload(m_FiBmp);
        m_FiBmp = fibmp;
    }

    operator FIBITMAP *() { return m_FiBmp; }
};

#endif

/// Arguments: image index and its determined translation vector
void c_ImageAlignmentWorkerThread::PhaseCorrImgTranslationCallback(int imgIdx, float Tx, float Ty)
{
    AlignmentEventPayload_t payload;
    payload.translation.x = Tx;
    payload.translation.y = Ty;
    //It is OK to pass a pointer to 'payload'. It will be dereferenced and the value copied before we return.
    SendMessageToParent(EID_PHASECORR_IMG_TRANSLATION, imgIdx, wxEmptyString, &payload);
}

/// Loads the specified input image, translates it and saves using the specified size and translation vector
bool c_ImageAlignmentWorkerThread::SaveTranslatedOutputImage(wxString inputFileName, int outputWidth, int outputHeight,
    float Tx, ///< X offset within the input image
    float Ty  ///< Y offset within the input image
    )
{
    // NOTE: all 'c_Image' and 'FIBITMAP' objects used in this method are wrapped
    // in "scoped pointer" objects, which destroy automatically (on leaving scope)
    // or by a reset()/Reset() call.
    // Therefore there's no need to clean them up when doing an early return.

#if USE_FREEIMAGE
    // Used by FreeImage_AllocateExT as "black" to initially fill a bitmap
    uint8_t zeros[4*sizeof(float)] = { 0 }; // works also for floating-point (pattern of all zero bits represents 0.0f)
#endif

    std::unique_ptr<c_Image> srcImg;
    std::unique_ptr<c_Image> outpImg;

    Log::Print(wxString::Format("Loading %s... ", inputFileName));

#if USE_FREEIMAGE
    c_ScopedFreeImageBmp fiSrcBmp, fiOutpBmp;
#endif

#if USE_CFITSIO
    bool isFits = false;
    wxString extension = wxFileName(inputFileName).GetExt().Lower();
    if (extension == "fit" || extension == "fits")
    {
        srcImg.reset(LoadFitsImage(inputFileName.ToStdString()));
        isFits = true;
        outpImg.reset(new c_Image(outputWidth, outputHeight, srcImg->GetPixelFormat()));
    }
    else
#endif
    {
#if USE_FREEIMAGE
        FREE_IMAGE_FORMAT fif = FIF_UNKNOWN;
        fif = FreeImage_GetFileType(inputFileName.c_str());
        if (FIF_UNKNOWN == fif)
            fif = FreeImage_GetFIFFromFilename(inputFileName.c_str());

        if (fif != FIF_UNKNOWN && FreeImage_FIFSupportsReading(fif))
        {
            fiSrcBmp.Reset(FreeImage_Load(fif, inputFileName.c_str()));
            if (!fiSrcBmp.Get())
            {
                m_CompletionMessage = wxString::Format(_("Could not read %s."), inputFileName);
                return false;
            }
        }
        else
        {
            m_CompletionMessage = wxString::Format(_("Unsupported file or image type: %s"), inputFileName);
            return false;
        }

        // Subpixel translation of palettised images is not supported, so convert to RGB8
        if (FreeImage_GetPalette(fiSrcBmp))
        {
            fiSrcBmp.Reset(FreeImage_ConvertTo24Bits(fiSrcBmp));
        }

        srcImg.reset(new c_Image(std::unique_ptr<IImageBuffer>(new c_FreeImageBuffer(fiSrcBmp))));
        if (srcImg->GetPixelFormat() == PIX_INVALID)
        {
            m_CompletionMessage = wxString::Format(_("Unsupported pixel format: %s"), inputFileName);
            return false;
        }

        fiOutpBmp.Reset(FreeImage_AllocateExT(
            FreeImage_GetImageType(fiSrcBmp),
            outputWidth, outputHeight,
            FreeImage_GetBPP(fiSrcBmp),
            zeros, 0, 0,
            FreeImage_GetRedMask(fiSrcBmp), FreeImage_GetGreenMask(fiSrcBmp), FreeImage_GetBlueMask(fiSrcBmp)));
        if (!fiOutpBmp)
        {
            m_CompletionMessage = _("FreeImage_AllocateExT failed to allocate an output bitmap.");
            return false;
        }

        outpImg.reset(new c_Image(std::unique_ptr<IImageBuffer>(new c_FreeImageBuffer(fiOutpBmp))));

#else
        wxString ext = wxFileName(inputFileName).GetExt().Lower();
        if (ext == "tif" || ext == "tiff")
            srcImg.reset(ReadTiff(inputFileName.c_str()));
        else if (ext == "bmp")
            srcImg.reset(ReadBmp(inputFileName.c_str()));

        if (!srcImg.get())
        {
            m_CompletionMessage = wxString::Format(_("Could not read %s."), inputFileName);
            return false;
        }

        // Subpixel translation of palettised images is not supported, so convert to RGB8
        if (srcImg->GetPixelFormat() == PIX_PAL8)
        {
            srcImg.reset(c_Image::ConvertPixelFormat(*srcImg, PIX_RGB8));
        }

        outpImg.reset(new c_Image(outputWidth, outputHeight, srcImg->GetPixelFormat()));
#endif
    }

    Log::Print("done. Allocated output image.\n");

    c_Image::ResizeAndTranslate(srcImg->GetBuffer(), outpImg->GetBuffer(), 0, 0, srcImg->GetWidth()-1, srcImg->GetHeight()-1, Tx, Ty, true);

    Log::Print("Created output image.\n");

    // Input image is no longer needed, free it
    srcImg.reset();
#if USE_FREEIMAGE
    fiSrcBmp.Reset();
#endif

    wxFileName fnInput(inputFileName);
    wxFileName outputFileName(m_Parameters.outputDir, fnInput.GetName() + "_aligned", fnInput.GetExt());
    bool saved = false;

#if USE_CFITSIO
    if (isFits)
    {
        OutputFormat_t outF = OUTF_FITS_16;
        switch (outpImg->GetPixelFormat())
        {
        case PIX_MONO8: outF = OUTF_FITS_8; break;
        case PIX_MONO16: outF = OUTF_FITS_16; break;
        case PIX_MONO32F: outF = OUTF_FITS_32F; break;
        }
        outputFileName.SetExt("fit");
        saved = SaveImageFile(outputFileName.GetFullPath().ToStdString(), *outpImg, outF);
    }
    else
#endif
    {
        outputFileName.SetExt("tif");

#if USE_FREEIMAGE
        saved = FreeImage_Save(FIF_TIFF, fiOutpBmp, outputFileName.GetFullPath().c_str(), TIFF_NONE);
        fiOutpBmp.Reset();
#else
        saved = SaveTiff(outputFileName.GetFullPath().c_str(), *outpImg.get());
#endif
    }

    Log::Print("Saved output image.\n");

    outpImg.reset();
    if (!saved)
    {
        m_CompletionMessage = wxString::Format(_("Failed to save output file: %s"), outputFileName.GetFullPath());
        return false;
    }

    return true;
}

/// Aligns the images by keeping the high-contrast features stationary
void c_ImageAlignmentWorkerThread::PhaseCorrelationAlignment()
{
    Log::Print("Started image alignment via phase correlation.\n");

    // Determine the largest width and height of all images
    unsigned maxWidth = 0, maxHeight = 0;

    std::vector<Point_t> imgSize; // Image sizes

    for (size_t i = 0; i < m_Parameters.inputFiles.Count(); i++)
    {
        unsigned width, height;
        if (!GetImageSize(m_Parameters.inputFiles[i].ToStdString(),
            wxFileName(m_Parameters.inputFiles[i].ToStdString()).GetExt().ToStdString(),
            width, height))
        {
            m_CompletionMessage = wxString::Format(_("Failed to obtain image dimensions from %s."), m_Parameters.inputFiles[i]);
            return;
        }

        imgSize.push_back(Point_t(width, height));

        if (width > maxWidth)
            maxWidth = width;
        if (height > maxHeight)
            maxHeight = height;
    }

    // Width and height of FFT arrays and the translation working buffer
    unsigned Nwidth = GetClosestGPowerOf2(maxWidth),
        Nheight = GetClosestGPowerOf2(maxHeight);

    std::vector<FloatPoint_t> translation;
    Rectangle_t bbox; // bounding box of all images after alignment

    if (!DetermineTranslationVectors(Nwidth, Nheight, m_Parameters.inputFiles,
        translation, bbox, &m_CompletionMessage, m_Parameters.subpixelAlignment,
        [this](int imgIdx, float tX, float tY) { PhaseCorrImgTranslationCallback(imgIdx, tX, tY); },
        [this]() { return IsAbortRequested(); }))
    {
        return;
    }

    Rectangle_t imgIntersection = DetermineImageIntersection(Nwidth, Nheight, bbox, translation, imgSize);

    // Iterate again over all images, load, pad to the bounding box size or crop to intersection, translate and save

    int outputWidth = m_Parameters.cropMode == CM_CROP_TO_INTERSECTION ? imgIntersection.width : bbox.width;
    int outputHeight = m_Parameters.cropMode == CM_CROP_TO_INTERSECTION ? imgIntersection.height : bbox.height;


    for (size_t i = 0; i < m_Parameters.inputFiles.Count(); i++)
    {
        if (IsAbortRequested())
            return;

        Point_t translationOrigin;
        if (m_Parameters.cropMode == CM_CROP_TO_INTERSECTION)
        {
            translationOrigin.x = imgIntersection.x;
            translationOrigin.y = imgIntersection.y;
        }
        else
        {
            translationOrigin.x = bbox.x;
            translationOrigin.y = bbox.y;
        }

        if (!SaveTranslatedOutputImage(m_Parameters.inputFiles[i], outputWidth, outputHeight,
            (Nwidth - imgSize[i].x)/2 - translation[i].x - translationOrigin.x,
            (Nheight - imgSize[i].y)/2 - translation[i].y - translationOrigin.y))
        {
            return;
        }

        SendMessageToParent(EID_SAVED_OUTPUT_IMAGE, i);
    }

    m_ProcessingCompleted = true;
}

/// Returns the quality of the specified image area: the sum of squared gradients
float GetQuality(c_Image &img, const Rectangle_t &area)
{
    assert(img.GetPixelFormat() == PIX_MONO32F);

    float result = 0;

    // Skip the border pixels in case there is a bright leftover from wavelet sharpening
    const int BORDER_SKIP = 3;

    for (int y = BORDER_SKIP; y < area.height - BORDER_SKIP - 1; y++)
        for (int x = BORDER_SKIP; x < area.width - BORDER_SKIP - 1; x++)
        {
            float val00 = ((float *)img.GetRow(area.y + y))[area.x + x];
            float val10 = ((float *)img.GetRow(area.y + y))[area.x + x+1];
            float val01 = ((float *)img.GetRow(area.y + y+1))[area.x + x];

            result += SQR(val10 - val00) + SQR(val01 - val00);
        }

    return result;
}

c_Image *GetBlurredImage(c_Image &srcImg, float gaussianSigma)
{
    assert(srcImg.GetPixelFormat() == PIX_MONO32F);

    c_Image *result = new c_Image(srcImg.GetWidth(), srcImg.GetHeight(), PIX_MONO32F);

    ConvolveSeparable(
            c_PaddedArrayPtr<float>((float *)srcImg.GetRow(0), srcImg.GetWidth(), srcImg.GetHeight(), srcImg.GetBuffer().GetBytesPerRow()),
            c_PaddedArrayPtr<float>((float *)result->GetRow(0), result->GetWidth(), result->GetHeight(), result->GetBuffer().GetBytesPerRow()),
            gaussianSigma);

    return result;
}

/// Performs the final stabilization phase of limb alignment; returns 'true' on success
bool c_ImageAlignmentWorkerThread::StabilizeLimbAlignment(
    /// Translation vectors to be corrected by stabilization
    std::vector<FloatPoint_t> &translation,
    /// Start of the images' intersection, relative to the first image's origin
    Point_t intersectionStart,
    int intrWidth, ///< Images' intersection width
    int intrHeight, ///< Images' intersection height
    wxString &errorMsg ///< Receives error message (if any)
    )
{
    // Simply finding the disc and overlapping it on subsequent images is often
    // not reliable and insufficient to achieve a smooth movement between frames.
    // We rectify this by doing what a human would do during a manual alignment:
    // tracking of some high-contrast feature as it travels across the disc (e.g. a sunspot)
    // or remains anchored at one point of the limb (e.g. a base of a prominence) and
    // smoothing out its motion.
    //
    // Current approach: assume that the chosen feature moves along a circular arc.
    // First, get the feature's position in each frame. Second, fit a circle to these
    // positions. Third, project the positions onto the circle and use them to correct
    // the translation vectors found so far during limb alignment (passed in 'translation').
    //
    // Note that the circular track assumption is just an approximation. If the feature
    // is a sunspot and there is no field rotation (just the solar rotation), its track
    // would be a circular or elliptical arc. With field rotation present the shape
    // would be more complicated.

    // 1. Select a high-contrast feature

    //TODO: (optional) display the first image and ask the user to select a feature that is
    //visible in every image

    // Size of the (square) stabilization area in pixels; has to be a power of 2
    const int STBL_AREA_SIZE = 128;

    // Size of the intersection area

    if (intrWidth >= STBL_AREA_SIZE && intrHeight >= STBL_AREA_SIZE)
    {
        // Center of the stabilization area (the high-contrast feature) in the first image
        // relative to the images' intersection origin
        Point_t stabilizationPos;

        // Scan the first image's intersection portion for the highest-contrast area
        std::unique_ptr<c_Image> firstImg(LoadImageFileAsMono32f(m_Parameters.inputFiles[0].ToStdString(), wxFileName(m_Parameters.inputFiles[0]).GetExt().Lower().ToStdString()));
        if (!firstImg.get())
        {
            errorMsg = wxString::Format(_("Could not read %s."), m_Parameters.inputFiles[0]);
            return false;
        }

        // Blur the image first to remove the impact of noise
        firstImg.reset(GetBlurredImage(*firstImg, 1.0f));

        float maxQuality = 0;
        for (int i = 0; i < intrWidth / (STBL_AREA_SIZE/2) - 1; i++)
            for (int j = 0; j < intrHeight / (STBL_AREA_SIZE/2) - 1; j++)
            {
                Rectangle_t currentArea(i*STBL_AREA_SIZE/2, j*STBL_AREA_SIZE/2, STBL_AREA_SIZE, STBL_AREA_SIZE);
                float quality = GetQuality(*firstImg,
                    Rectangle_t(intersectionStart.x + currentArea.x,
                                intersectionStart.y + currentArea.y,
                                currentArea.width, currentArea.height));
                if (quality > maxQuality)
                {
                    maxQuality = quality;
                    stabilizationPos = Point_t(currentArea.x + currentArea.width/2, currentArea.y + currentArea.height/2);
                }
            }

#if DEBUG_OUTPUT
        {
            // Save the selected area to an image
            c_Image *areaImg = new c_Image(STBL_AREA_SIZE, STBL_AREA_SIZE, PIX_MONO32F);
            c_Image::Copy(*firstImg, *areaImg,
                intersectionStart.x + stabilizationPos.x - STBL_AREA_SIZE/2,
                intersectionStart.y + stabilizationPos.y - STBL_AREA_SIZE/2,
                STBL_AREA_SIZE, STBL_AREA_SIZE, 0, 0);
            SaveImageFile("area.bmp", *areaImg, OUTF_BMP_MONO_8);
            delete areaImg;
        }
#endif

        // 2. Trace the movement of the stabilization area

        // Window function for blunting the stabilization area's edges; Has a 1.0 peak in the middle and tapers to zero near the edges
        std::unique_ptr<c_Image> wndFunc(CalcWindowFunction(STBL_AREA_SIZE, STBL_AREA_SIZE));

        // Each element is the position of the stabilization area in subsequent images relative to the images' intersection origin
        std::vector<FloatPoint_t> stAreaImagePos;
        stAreaImagePos.push_back(FloatPoint_t(stabilizationPos.x, stabilizationPos.y));

        // Stabilization area in the previous (currently: first) image
        std::unique_ptr<c_Image> prevArea(new c_Image(STBL_AREA_SIZE, STBL_AREA_SIZE, PIX_MONO32F));
        c_Image::Copy(*firstImg, *prevArea,
            intersectionStart.x + stabilizationPos.x - STBL_AREA_SIZE/2,
            intersectionStart.y + stabilizationPos.y - STBL_AREA_SIZE/2,
            STBL_AREA_SIZE, STBL_AREA_SIZE, 0, 0);
        prevArea->Multiply(*wndFunc);

        firstImg.reset();

        FloatPoint_t prevFrac(0.0f, 0.0f);

        SendMessageToParent(EID_LIMB_STABILIZATION_PROGRESS, 0);

        for (size_t i = 1; i < m_Parameters.inputFiles.Count(); i++)
        {
            if (IsAbortRequested())
                return false;

            SendMessageToParent(EID_LIMB_STABILIZATION_PROGRESS, i);

            std::unique_ptr<c_Image> currImg(LoadImageFileAsMono32f(m_Parameters.inputFiles[i].ToStdString(), wxFileName(m_Parameters.inputFiles[i]).GetExt().Lower().ToStdString()));
            if (!currImg.get())
            {
                errorMsg = wxString::Format(_("Could not read %s."), m_Parameters.inputFiles[i]);
                return false;
            }

            Point_t Tint;
            FloatPoint_t Tfrac;
            float temp;
            Tfrac.x = std::modf(translation[i].x, &temp); Tint.x = (int)temp;
            Tfrac.y = std::modf(translation[i].y, &temp); Tint.y = (int)temp;

            // Stabilization area in the current image (assumed to be at the same position - relative to the images' intersection - as in the previous image)
            std::unique_ptr<c_Image> currArea(new c_Image(STBL_AREA_SIZE, STBL_AREA_SIZE, PIX_MONO32F));
            c_Image::Copy(*currImg, *currArea,
                intersectionStart.x - Tint.x + stabilizationPos.x - STBL_AREA_SIZE/2,
                intersectionStart.y - Tint.y + stabilizationPos.y - STBL_AREA_SIZE/2,
                STBL_AREA_SIZE, STBL_AREA_SIZE, 0, 0);

            currArea->Multiply(*wndFunc);

            FloatPoint_t areaTranslation = DetermineTranslationVector(*prevArea, *currArea);
            FloatPoint_t &prev = stAreaImagePos.back();

            // We also need to take the fractional parts of current and previous translation into account,
            // because the 'prevArea' and 'currArea' sub-images (which we use to obtain 'areaTranslation')
            // start (obviously) at integer pixel coordinates.
            areaTranslation.x += Tfrac.x - prevFrac.x;
            areaTranslation.y += Tfrac.y - prevFrac.y;

            stAreaImagePos.push_back(FloatPoint_t(prev.x + areaTranslation.x, prev.y + areaTranslation.y));

            // Update position of the stabilization area for next iteration
            Point_t newStabilizationPos = Point_t(stabilizationPos.x + (int)areaTranslation.x,
                stabilizationPos.y + (int)areaTranslation.y);
            //Make sure 'stabilizationPos' didn't go outside the images' intersection
            if (newStabilizationPos.x - STBL_AREA_SIZE/2 >= 0 &&
                newStabilizationPos.y - STBL_AREA_SIZE/2 >= 0 &&
                newStabilizationPos.x + STBL_AREA_SIZE/2 < intrWidth &&
                newStabilizationPos.y + STBL_AREA_SIZE/2 < intrHeight)
            {
                stabilizationPos = newStabilizationPos;
            }

            // Refresh the current area using the updated stabilization position
            c_Image::Copy(*currImg, *currArea,
                intersectionStart.x - Tint.x + stabilizationPos.x - STBL_AREA_SIZE/2,
                intersectionStart.y - Tint.y + stabilizationPos.y - STBL_AREA_SIZE/2,
                STBL_AREA_SIZE, STBL_AREA_SIZE, 0, 0);
            currArea->Multiply(*wndFunc);

            prevArea.swap(currArea);

            prevFrac = Tfrac;
        }

        // 3. Smooth out the movement of stabilization area (currently: project it onto a circular arc)

        // Smoothed out track of the stabilization area - a circular arc
        struct
        {
            float cx, cy, r;
        } smoothTrack;

        if (!FitCircleToPoints(stAreaImagePos, &smoothTrack.cx, &smoothTrack.cy, &smoothTrack.r))
        {
            errorMsg = _("Could not fit a circle to points during final stabilization.");
            return false;
        }

        /*
        Points in 'stAreaImagePos' will likely be spaced untidily, e.g.:

                   (4)
        (1)                  (5)
        ---------------------------------   <- fitted circular arc
                     (3)          (6)
             (2)

        In the above example the long-term direction of stabilization area's movement
        is left to right, but notice how point (4) jumps back with respect to point (3).
        After projecting the points onto the track we would have:

        -1----2-----4-3-------5----6-----   <- fitted circular arc

        The 3->4 transition would still cause an animation jitter. To avoid it,
        we make sure the points are sorted along the track. In a case like above,
        we shall move point 4 to be the same as 3.
        */

        // Determine the long-term direction of movement (clockwise or counter-clockwise
        // along the circular track) by calculating the cross product of vectors pointing
        // from the track's center to the first and last point.

        FloatPoint_t vFirst(stAreaImagePos[0].x - smoothTrack.cx,
            stAreaImagePos[0].y - smoothTrack.cy);
        FloatPoint_t vLast(stAreaImagePos.back().x - smoothTrack.cx,
            stAreaImagePos.back().y - smoothTrack.cy);

        // We use 2D vectors, so the cross product has a single non-zero component (Z);
        // let us treat it as a scalar.
        float firstLastCrossProd = vFirst.x*vLast.y - vFirst.y*vLast.x;

        FloatPoint_t prevProj(0.0f, 0.0f);

        for (unsigned i = 0; i < stAreaImagePos.size(); i++)
        {
            // Project current point onto the circle defined by 'smoothTrack'
            FloatPoint_t &p = stAreaImagePos[i];

            FloatPoint_t correction(0.0f, 0.0f); // difference after projection

            float len = std::sqrt(SQR(p.x - smoothTrack.cx) + SQR(p.y - smoothTrack.cy));
            if (len > 1.0e-8f)
            {
                FloatPoint_t proj(smoothTrack.r * (p.x - smoothTrack.cx) / len + smoothTrack.cx,
                    smoothTrack.r * (p.y - smoothTrack.cy) / len + smoothTrack.cy);

                if (i >= 1)
                {
                    // Make sure the current point is not behind the previous point on the circle,
                    // i.e. that the cross product of their pointing vectors has the same sign
                    // as 'firstLastCrossProd' (their product is positive).

                    float crossProd = (prevProj.x - smoothTrack.cx) * (p.y - smoothTrack.cy) -
                        (prevProj.y - smoothTrack.cy) * (p.x - smoothTrack.cx);
                    if (crossProd * firstLastCrossProd < 0.0f)
                    {
                        proj = prevProj;
                    }
                }

                correction.x = proj.x - p.x;
                correction.y = proj.y - p.y;

                prevProj = proj;
            }

            // Use the projected point to correct the value in 'translation'
            translation[i].x += correction.x;
            translation[i].y += correction.y;
        }
    }

    return true;
}

/// Returns number of neighbors of 'p' within 'radius' which have value > threshold
void CountNeighborsAboveThreshold(
    FloatPoint_t &p, c_Image &img, int radius, uint8_t threshold,
    size_t &numAbove, ///< Receives the number of pixels above threshold
    size_t &numTotal ///< Receives the number of all neighbor pixels used
)
{
    numAbove = 0; numTotal = 0;
    for (int y = std::max((int)(p.y - radius), 0); y <= std::min((int)(p.y + radius), (int)img.GetHeight()-1); y++)
        for (int x = std::max((int)(p.x - radius), 0); x <= std::min((int)(p.x + radius), (int)img.GetWidth()-1); x++)
            if (SQR(x - p.x) + SQR(y - p.y) <= SQR(radius))
            {
                numTotal++;
                if (((uint8_t *)img.GetRow(y))[x] > threshold)
                    numAbove++;
            }
}

/// Finds disc radii in input images; returns 'true' on success
bool c_ImageAlignmentWorkerThread::FindRadii(
    std::vector<std::vector<FloatPoint_t> > &limbPoints, ///< Receives limb points found in n-th image
    std::vector<float> &radii, ///< Receives disc radii determined for each image
    std::vector<Point_t> &imgSizes, ///< Receives input image sizes
    std::vector<Point_t> &centroids ///< Receives image centroids
)
{
    AlignmentEventPayload_t payload;

    for (size_t i = 0; i < m_Parameters.inputFiles.Count(); i++)
    {
        if (IsAbortRequested())
            return false;

        std::unique_ptr<c_Image> img(LoadImageFileAsMono8(m_Parameters.inputFiles[i].ToStdString(), wxFileName(m_Parameters.inputFiles[i]).GetExt().Lower().ToStdString()));
        if (!img.get())
        {
            m_CompletionMessage = wxString::Format(_("Could not read %s."), m_Parameters.inputFiles[i]);
            return false;
        }

        imgSizes.push_back(Point_t(img->GetWidth(), img->GetHeight()));

        // 1. Find the threshold value of brightness which separates
        //      the disc from the background.

        uint8_t avgDisc, avgBkgrnd;
        uint8_t threshold = FindDiscBackgroundThreshold(*img, &avgDisc, &avgBkgrnd);

        // 2. Calculate the image centroid

        Point_t centroid = CalcCentroid(*img);
        centroids.push_back(centroid);

        // 3. Trace a number of rays originating at the centroid

        const int NUM_RAYS = 64; //TODO: make it configurable
        std::unique_ptr<Ray_t[]> rays(new Ray_t[NUM_RAYS]);

        for (int j = 0; j < NUM_RAYS; j++)
        {
            Point_t dir;
            dir.x = NUM_RAYS * std::cos(j * 2*3.14159f / NUM_RAYS);
            dir.y = NUM_RAYS * std::sin(j * 2*3.14159f / NUM_RAYS);
            GetRayPoints(centroid, dir, *img, rays[j]);
        }

        // 4. Find limb crossing points along 'rays'

        // Key: steepness of transition (higher = better)
        std::multimap<int, Point_t> limbPointsCandidates;
        for (int j = 0; j < NUM_RAYS; j++)
        {
            Point_t limbPt;
            int varSum = FindLimbCrossing(rays[j], *img, threshold, limbPt);
            limbPointsCandidates.insert(std::pair<int, Point_t>(varSum, limbPt));
        }

        const int THRESHOLD_DIV = 3;
        // 4.1. Some of the points may be misidentified (e.g. an edge of prominence or a sunspot).
        // Assume that if the point's steepness is less than 1/THRESHOLD_DIV of the expected avg steepness,
        // it is not in fact a limb point - and discard it.

        int avgSteepness = DIFF_SIZE * ((int)avgDisc - avgBkgrnd);

        // A multimap is sorted by keys ascending and we are interested in the steepest transitions,
        // so start the iteration from the last element ('rbegin')
        for (std::multimap<int, Point_t>::const_reverse_iterator rIt = limbPointsCandidates.rbegin();
            rIt != limbPointsCandidates.rend();
            rIt++)
        {
            if (rIt->first >= 1*avgSteepness/THRESHOLD_DIV)
                limbPoints[i].push_back(FloatPoint_t(rIt->second.x, rIt->second.y));
            else
                break;
        }

        // 4.2. The previous step is sometimes insufficient to remove non-limb points (e.g. if a point was detected
        //      on the edge of a wide dark filament). Perform the final verification step: the point lies on the limb
        //      if a sufficient fraction of its neighbors has values below the disc/background threshold.
        //
        //      In an ideal, simplified situation the "above threshold" fraction should be always < 0.5 (as the disc
        //      is convex). In practice, it may be higher (e.g. if we have an overexposed disc with quite bright halo
        //      + prominences).
        //

        // Max. fraction of "above threshold" neighbors which is acceptable for a limb point
        const float MAX_ABOVE_THRESHOLD_FRACTION = 0.6f;

        std::vector<float> aboveThFraction;
        size_t numPointsExceedingMaxFraction = 0;
        for (size_t j = 0; j < limbPoints[i].size(); j++)
        {
            size_t numTotal, numAbove;
            int radius = DIFF_SIZE;
            CountNeighborsAboveThreshold(limbPoints[i][j], *img, radius, threshold, numAbove, numTotal);

            float fraction = (float)numAbove/numTotal;
            aboveThFraction.push_back(fraction);

            if (fraction > MAX_ABOVE_THRESHOLD_FRACTION)
                numPointsExceedingMaxFraction++;
        }

        // Assume that if more than 3/4 of points have the "above threshold" fraction exceeding the limit,
        // we are dealing with an overexposed disc. In this case, do not remove any points.
        if (numPointsExceedingMaxFraction < 3*limbPoints[i].size()/4)
        {
            // Not an overexposed disc, so the fractions are fine; remove points which exceed the limit
            for (int j = (int)limbPoints[i].size() - 1; j >= 0; j--)
                if (aboveThFraction[j] > MAX_ABOVE_THRESHOLD_FRACTION)
                    limbPoints[i].erase(limbPoints[i].begin() + j);
        }

        Log::Print(wxString::Format("Found %d limb point candidates, used %d (%d%%).\n",
            (int)limbPointsCandidates.size(), (int)limbPoints[i].size(), (int)(100*limbPoints[i].size()/limbPointsCandidates.size())));

        if (limbPoints[i].size() < 3)
        {
            m_CompletionMessage = wxString::Format(_("Could not find the limb in %s."), m_Parameters.inputFiles[i]);
            return false;
        }

        // 5. Determine disc radius. Theoretically it is the same in each image, in practice
        //      the radii will differ due to seeing, stacking and post-processing variability.

        float radius, cx = centroid.x, cy = centroid.y;
        if (!FitCircleToPoints(limbPoints[i], &cx, &cy, &radius, 0.0f, true))
        {
            m_CompletionMessage = wxString::Format(_("Could not find the limb in %s."), m_Parameters.inputFiles[i]);
            return false;
        }
        radii.push_back(radius);


        payload.radius = radius;
        SendMessageToParent(EID_LIMB_FOUND_DISC_RADIUS, i, wxEmptyString, &payload);
    }

    return true;
}

/// Aligns the images by keeping the limb stationary
void c_ImageAlignmentWorkerThread::LimbAlignment()
{
    // NOTE: all 'c_Image' and 'FIBITMAP' objects used in this method are wrapped
    // in "scoped pointer" objects, which destroy automatically (on leaving scope)
    // or by a reset()/Reset() call.
    // Therefore there's no need to clean them up when doing an early return.

    Log::Print("Started image alignment using the limb as anchor.\n");

    AlignmentEventPayload_t payload;

    std::vector<std::vector<FloatPoint_t> > limbPoints(m_Parameters.inputFiles.Count()); // n-th element is a list of limb points found in n-th image
    std::vector<float> radii; // disc radius determined for each image
    std::vector<Point_t> imgSizes;
    std::vector<Point_t> centroids;

    // 1. Determine disc radius in each image

    if (!FindRadii(limbPoints, radii, imgSizes, centroids))
        return;

    // 2. Calculate average radius and use it to fit discs to the limb points again

    float avgRadius = 0, maxRadius = -FLT_MAX, minRadius = FLT_MAX;
    for (unsigned i = 0; i < radii.size(); i++)
    {
        if (radii[i] > maxRadius)
            maxRadius = radii[i];
        if (radii[i] < minRadius)
            minRadius = radii[i];

        avgRadius += radii[i];
    }
    avgRadius /= radii.size();

    if (maxRadius / minRadius > 1.5f)
    {
        // If the min. and max. radii differ too much, we have misidentified limb points somewhere,
        // possibly due to vignetting or exaggerated limb darkening present.
        m_CompletionMessage = _("Could not determine valid disc radius in every image.");
        return;
    }

    payload.radius = avgRadius;
    SendMessageToParent(EID_LIMB_USING_RADIUS, 0, wxEmptyString, &payload);

    std::vector<FloatPoint_t> translation; // i-th element is the required translation of i-th image relative to the first image; the first element is (0, 0)
    double cx0, cy0; // disc center relative to the first image's origin (upper-left corner)

    for (unsigned i = 0; i < limbPoints.size(); i++)
    {
        if (IsAbortRequested())
            return;

        float cx = centroids[i].x, cy = centroids[i].y;
        if (!FitCircleToPoints(limbPoints[i], &cx, &cy, 0, avgRadius, true))
        {
            m_CompletionMessage = wxString::Format(_("Could not find the limb with forced radius in %s."), m_Parameters.inputFiles[i]);
            return;
        }
        if (i == 0)
        {
            cx0 = cx;
            cy0 = cy;
        }

        translation.push_back(FloatPoint_t(cx0 - cx, cy0 - cy));
    }

    // 3. Determine the bounding box and intersection of all images after their translation

    // Coordinates in both structures are relative to the first image's origin
    struct
    {
        int xmin, xmax, ymin, ymax;
    } bbox = { INT_MAX, INT_MIN, INT_MAX, INT_MIN },
      intersection = { INT_MIN, INT_MAX, INT_MIN, INT_MAX };

    for (unsigned i = 0; i < translation.size(); i++)
    {
        // Note: rounding via an (int) cast in order to round towards zero,
        // because that's how c_Image::ResizeAndTranslate() also operates.
        int tx = (int)translation[i].x,
            ty = (int)translation[i].y;

        if (tx < bbox.xmin)
            bbox.xmin = tx;
        if (tx + imgSizes[i].x > bbox.xmax)
            bbox.xmax = tx + imgSizes[i].x;

        if (ty < bbox.ymin)
            bbox.ymin = ty;
        if (ty + imgSizes[i].y > bbox.ymax)
            bbox.ymax = ty + imgSizes[i].y;

        if (tx > intersection.xmin)
            intersection.xmin = tx;
        if (tx + imgSizes[i].x < intersection.xmax)
            intersection.xmax = tx + imgSizes[i].x;

        if (ty > intersection.ymin)
            intersection.ymin = ty;
        if (ty + imgSizes[i].y < intersection.ymax)
            intersection.ymax = ty + imgSizes[i].y;
    }

    // 4. Perform additional stabilization

    // Failure of this method is not considered critical
    wxString stabilizationErrorMsg;
    if (!StabilizeLimbAlignment(translation, Point_t(intersection.xmin, intersection.ymin),
        intersection.xmax - intersection.xmin + 1,
        intersection.ymax - intersection.ymin + 1,
        stabilizationErrorMsg))
    {
        SendMessageToParent(EID_LIMB_STABILIZATION_FAILURE, 0, stabilizationErrorMsg);
    }

    // 5. Load images again, pad them to bounding box or crop to intersection and save

    int outputWidth, outputHeight;
    if (m_Parameters.cropMode == CM_PAD_TO_BOUNDING_BOX)
    {
        outputWidth = bbox.xmax - bbox.xmin + 1;
        outputHeight = bbox.ymax - bbox.ymin + 1;
    }
    else
    {
        outputWidth = intersection.xmax - intersection.xmin + 1;
        outputHeight = intersection.ymax - intersection.ymin + 1;
    }

    for (size_t i = 0; i < m_Parameters.inputFiles.Count(); i++)
    {
        if (IsAbortRequested())
            return;

        float Tx, Ty;
        if (m_Parameters.cropMode == CM_PAD_TO_BOUNDING_BOX)
        {
            Tx = translation[i].x;
            Ty = translation[i].y;
        }
        else
        {
            Tx = translation[i].x - intersection.xmin;
            Ty = translation[i].y - intersection.ymin;
        }

        if (!m_Parameters.subpixelAlignment)
        {
            Tx = boost::math::round(Tx);
            Ty = boost::math::round(Ty);
        }

        if (!SaveTranslatedOutputImage(m_Parameters.inputFiles[i], outputWidth, outputHeight, Tx, Ty))
            return;

        SendMessageToParent(EID_SAVED_OUTPUT_IMAGE, i);
    }

    m_ProcessingCompleted = true;
}

wxThread::ExitCode c_ImageAlignmentWorkerThread::Entry()
{
    switch (m_Parameters.alignmentMethod)
    {
    case ALM_PHASE_CORRELATION: PhaseCorrelationAlignment(); break;
    case ALM_LIMB: LimbAlignment(); break;
    }

    return 0;
}

void c_ImageAlignmentWorkerThread::SendMessageToParent(int id, int value, wxString msg, AlignmentEventPayload_t *payload)
{
    wxThreadEvent *event = new wxThreadEvent(wxEVT_THREAD, id);
    event->SetInt(value);
    event->SetString(msg);
    if (payload)
        event->SetPayload<AlignmentEventPayload_t>(*payload);

    m_Parent.GetEventHandler()->QueueEvent(event);
}

/// Signals the thread to finish processing ASAP
void c_ImageAlignmentWorkerThread::AbortProcessing()
{
    m_AbortReq.Post();
}

c_ImageAlignmentWorkerThread::~c_ImageAlignmentWorkerThread()
{
    if (m_ProcessingCompleted)
        SendMessageToParent(EID_COMPLETED);
    else
        SendMessageToParent(EID_ABORTED, m_ThreadAborted ? ABRT_REQUESTED : ABRT_PROC_ERROR, m_CompletionMessage);
    { wxCriticalSectionLocker lock(m_Guard);
        *m_InstancePtr = 0;
    }
}

bool c_ImageAlignmentWorkerThread::IsAbortRequested()
{
    if (m_ThreadAborted)
        return true;
    else if (TestDestroy() ||
        (wxSEMA_NO_ERROR == m_AbortReq.TryWait())) // Check if the parent has called Post() on the semaphore
    {
        m_ThreadAborted = true;
        m_CompletionMessage = _("Aborted per user request.");
        return true;
    }
    else
    {
        return false;
    }
}
