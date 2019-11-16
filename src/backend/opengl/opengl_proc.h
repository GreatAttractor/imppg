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
#include "backend/opengl/gl_utils.h"
#include "image.h"

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
    c_OpenGLProcessing();

    void SetImage(const c_Image& img, bool linearInterpolation);

    void SetSelection(wxRect selection);

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
            return &m_TexFBOs.unsharpMask.tex;
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
    std::optional<c_Image> m_OwnedImg; ///< Set with `IProcessingBackEnd::StartProcessing`.
    std::optional<c_Image> m_ProcessedOutput;

    const c_Image* m_Img{nullptr}; /// < Image being processed (may point to `m_OwnedImg`).

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

    // All textures are GL_TEXTURE_RECTANGLE, GL_RED, GL_FLOAT (single-component 32-bit floating-point).

    gl::c_Texture m_OriginalImg;

    /// Framebuffer object and its associated texture; rendering to the FBO fills the texture.
    struct TexFbo
    {
        gl::c_Texture tex;
        gl::c_Framebuffer fbo;
    };

    // Textures & FBOs below have the same size as `m_Selection`.
    struct
    {
        TexFbo gaussianBlur; ///< Result of Gaussian blur of `lrSharpened`.
        TexFbo aux;
        TexFbo lrSharpened; ///< Result of L-R deconvolution sharpening of the original image.
        TexFbo toneCurve; ///< Result of sharpening, unsharp masking and tone mapping.
        TexFbo unsharpMask; ///< Result of sharpening and unsharp masking.

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
    std::vector<float> m_UnshMaskGaussian; // element [0] = max value

    /// Convolves `src` with `gaussian` and writes the output to `dest`.
    ///
    /// Does not bind VBOs nor activates vertex attribute pointers.
    ///
    void GaussianConvolution(gl::c_Texture& src, gl::c_Framebuffer& dest, const std::vector<float>& gaussian);

    /// Does not bind VBOs nor activates vertex attribute pointers.
    void MultiplyTextures(gl::c_Texture& tex1, gl::c_Texture& tex2, gl::c_Framebuffer& dest);

    /// Does not bind VBOs nor activates vertex attribute pointers.
    void DivideTextures(gl::c_Texture& tex1, gl::c_Texture& tex2, gl::c_Framebuffer& dest);

    /// Reinitializes texture and FBO if their size differs from `size`.
    void InitTextureAndFBO(TexFbo& texFbo, const wxSize& size);

    /// Issues an OpenGL command batch performing L-R iterations.
    void IssueLRCommandBatch();

    void StartLRDeconvolution();

    void StartUnsharpMasking();

    void StartToneMapping();

    void FillWholeSelectionVBO();
};

} // namespace imppg::backend

#endif // IMPPG_GL_PROCESSING_BACKEND_H
