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
    OpenGL processing back end declaration.
*/

#ifndef IMPPG_GL_PROCESSING_BACKEND_H
#define IMPPG_GL_PROCESSING_BACKEND_H

#include "backend/backend.h"
#include "common/common.h"
#include "opengl/gl_utils.h"
#include "image/image.h"

#include <array>
#include <vector>

namespace imppg::backend {

class c_OpenGLProcessing: public IProcessingBackEnd
{
public:
    // IProcessingBackEnd functions ---------------------------------------------------------------

    void StartProcessing(c_Image img, ProcessingSettings procSettings) override;

    void SetProcessingCompletedHandler(std::function<void(CompletionStatus)> handler) override;

    void SetProgressTextHandler(std::function<void(wxString)> handler) override;

    const c_Image& GetProcessedOutput() override;

    void OnIdle(wxIdleEvent& event) override;

    void AbortProcessing() override;

    // --------------------------------------------------------------------------------------------

    /// Can be called only if a current OpenGL context already exists and GLEW has been initialized
    /// (e.g., `c_OpenGLDisplay` has been constructed and its `MainWindowShown` method called).
    c_OpenGLProcessing(unsigned lRCmdBatchSizeMpixIters);

    void SetImage(c_Image img, bool linearInterpolation);

    void SetSelection(wxRect selection, bool skipBlurForDeringing = false);

    void SetProcessingSettings(ProcessingSettings settings);

    void StartProcessing(ProcessingRequest request);

    const gl::c_Texture* GetToneMappingOutput() const
    {
        if (m_ProcessingOutputValid.toneCurve)
        {
            return &m_TexFBOs.toneCurve.tex;
        }
        else
        {
            return nullptr;
        }
    }

    const gl::c_Texture* GetUsharpMaskOutput() const
    {
        if (m_ProcessingOutputValid.unshMask)
        {
            return &checked_back(m_TexFBOs.unsharpMask).tex;
        }
        else
        {
            return nullptr;
        }
    }

    const gl::c_Texture& GetOriginalImg() const { return m_OriginalImg; }

    c_Image GetProcessedSelection();

    /// Does not concern processing, only display by `c_OpenGLDisplay`.
    void SetTexturesLinearInterpolation(bool enable);

    bool IsProcessingInProgress() const { return (!m_LRSync.abortRequested && m_LRSync.numIterationsLeft > 0); }

private:
    std::optional<c_Image> m_Img; /// < Image being processed.

    std::optional<c_Image> m_ProcessedOutput;

    ProcessingSettings m_ProcessingSettings{};

    wxRect m_Selection;

    /// Does not concern processing, only display by `c_OpenGLDisplay`.
    bool m_LinearInterpolationForDisplay{false};

    std::function<void(wxString)> m_ProgressTextHandler;

    std::function<void(CompletionStatus)> m_ProcessingCompletedHandler;

    struct
    {
        struct
        {
            gl::c_Shader copy;
            gl::c_Shader toneCurve;
            gl::c_Shader gaussianHorz;
            gl::c_Shader gaussianVert;
            gl::c_Shader unsharpMask;
            gl::c_Shader divide;
            gl::c_Shader multiply;
        } frag;

        struct
        {
            gl::c_Shader passthrough;
        } vert;
    } m_GLShaders;

    struct
    {
        gl::c_Program copy;
        gl::c_Program toneCurve;
        gl::c_Program gaussianHorz;
        gl::c_Program gaussianVert;
        gl::c_Program unsharpMask;
        gl::c_Program multiply;
        gl::c_Program divide;
    } m_GLPrograms;

    // All textures are GL_TEXTURE_RECTANGLE, GL_RED or GL_RGB, GL_FLOAT (1- or 3-component 32-bit floating-point).

    gl::c_Texture m_OriginalImg;


    // Original image with pixels above `DERINGING_BRIGHTNESS_THRESHOLD` (and their vicinity) blurred to avoid ringing.
    struct
    {
        std::optional<c_Image> image;
        gl::c_Texture texture;
        std::vector<uint8_t> workBuf;
    } m_BlurredForDeringing;

    /// Framebuffer object and its associated texture; rendering to the FBO fills the texture.
    struct TexFbo
    {
        gl::c_Texture tex;
        gl::c_Framebuffer fbo;
    };

    // Textures & FBOs below have the same size as `m_Selection`.
    struct
    {
        /// Gaussian-blurred input image to provide "steering brightness" during adaptive unsharp masking.
        TexFbo inputBlurred;
        TexFbo gaussianBlur; ///< Result of Gaussian blur of `lrSharpened`.
        TexFbo aux;
        TexFbo lrSharpened; ///< Result of L-R deconvolution sharpening of the original image.
        TexFbo toneCurve; ///< Result of sharpening, unsharp masking and tone mapping.
        /// Results of sharpening and unsharp masking. By convention, there is always at least one element,
        /// even if unsharp masking is a no-op (i.e., amount = 1.0).
        std::vector<TexFbo> unsharpMask;

        /// Lucy-Richardson deconvolution intermediate data.
        struct
        {
            TexFbo original;
            TexFbo buf1;
            TexFbo buf2;
            TexFbo estimateConvolved;
            TexFbo convolvedDiv;
            TexFbo convolved2;
        } LR;
    } m_TexFBOs;

    struct
    {
        gl::c_Buffer wholeSelection; ///< Vertices specify a full-screen quad, tex. coords correspond to `m_Selection`.
        gl::c_Buffer wholeSelectionAtZero; ///< Vertices specify a full-screen quad, tex. coords correspond to `m_Selection` positioned at (0, 0).
    } m_VBOs;

    // Lucy-Richardson deconvolution work issuing and synchronization.
    struct
    {
        bool abortRequested = false;
        unsigned numIterationsLeft = 0;
        TexFbo* prev = nullptr;
        TexFbo* next = nullptr;
    } m_LRSync;

    /// Indicates if all OpenGL commands required for the processing step have been submitted for execution;
    /// if `true`, any new commands can rely on `m_Textures` containing valid output.
    struct
    {
        bool sharpening{false};
        bool unshMask{false};
        bool toneCurve{false};
    } m_ProcessingOutputValid;

    std::vector<float> m_LRGaussian; // element [0] = max value

    struct UnsharpMaskData
    {
        std::vector<float> gaussian; // element [0] = max value

        /// Coefficients of cubic transition curve for adaptive unsharp masking
        /// (see `GetAdaptiveUnshMaskTransitionCurve` in `common.h` for details).
        std::array<float, 4> transitionCurve;
    };

    std::vector<UnsharpMaskData> m_UnshMaskData; // data for each unsharp mask

    /// Number of megapixel-iterations of L-R deconvolution to perform in a single OpenGL command batch.
    const unsigned m_LRCmdBatchSizeMpixIters;

    /// Convolves `src` with `gaussian` and writes the output to `dest`.
    ///
    /// Does not bind VBOs nor activates vertex attribute pointers.
    ///
    void GaussianConvolution(const gl::c_Texture& src, gl::c_Framebuffer& dest, const std::vector<float>& gaussian);

    /// Does not bind VBOs nor activates vertex attribute pointers.
    void MultiplyTextures(const gl::c_Texture& tex1, const gl::c_Texture& tex2, gl::c_Framebuffer& dest);

    /// Does not bind VBOs nor activates vertex attribute pointers.
    void DivideTextures(const gl::c_Texture& tex1, const gl::c_Texture& tex2, gl::c_Framebuffer& dest);

    /// Reinitializes texture and FBO if their size differs from `size` (in that case, returns `true`).
    bool InitTextureAndFBO(TexFbo& texFbo, const wxSize& size);

    /// Issues an OpenGL command batch performing L-R iterations.
    void IssueLRCommandBatch();

    void StartLRDeconvolution();

    void StartUnsharpMasking(std::size_t maskIdx);

    void StartToneMapping();

    void FillSelectionVBOs();

    void BlurForDeringing(double sigma);
};

} // namespace imppg::backend

#endif // IMPPG_GL_PROCESSING_BACKEND_H
