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
    OpenGL back end declaration.
*/

#include <functional>
#include <optional>
#include <memory>
#include <wx/bitmap.h>
#include <wx/event.h>

#include "../../common.h"
#include "../../scrolled_view.h"
#include "../backend.h"
#include "gl_utils.h"

#include <wx/glcanvas.h> // has to be below other OpenGL-related includes

namespace imppg::backend {

class c_OpenGLBackEnd: public IBackEnd
{
public:
    /// Returns `nullptr` if OpenGL cannot be initialized.
    static std::unique_ptr<c_OpenGLBackEnd> Create(c_ScrolledView& imgView);

    void ImageViewScrolledOrResized(float zoomFactor) override;

    void ImageViewZoomChanged(float zoomFactor) override;

    void FileOpened(c_Image&& img, std::optional<wxRect> newSelection) override;

    Histogram GetHistogram() override;

    void SetPhysicalSelectionGetter(std::function<wxRect()> getter) override { m_PhysSelectionGetter = getter; }

    void SetScaledLogicalSelectionGetter(std::function<wxRect()>) override {}

    void SetProcessingCompletedHandler(std::function<void()> handler) override { m_OnProcessingCompleted = handler; }

    void NewSelection(const wxRect& selection) override;

    bool MainWindowShown() override;

    void RefreshRect(const wxRect&) override { m_ImgView.GetContentsPanel().Refresh(false); }

    void NewProcessingSettings(const ProcessingSettings& procSettings) override;

    void LRSettingsChanged(const ProcessingSettings& procSettings) override;

    void UnshMaskSettingsChanged(const ProcessingSettings& procSettings) override;

    void ToneCurveChanged(const ProcessingSettings& procSettings) override;

    void OnIdle(wxIdleEvent& event) override;

    void SetScalingMethod(ScalingMethod scalingMethod) override;

private:
    c_ScrolledView& m_ImgView;

    wxGLCanvas* m_GLCanvas{nullptr};

    std::unique_ptr<wxGLContext> m_GLContext;

    std::optional<c_Image> m_Img;

    float m_ZoomFactor{ZOOM_NONE};

    float m_NewZoomFactor{ZOOM_NONE};

    wxRect m_Selection; ///< Image fragment selected for processing (in logical image coords).

    /// Provides selection in physical image view coords.
    std::function<wxRect()> m_PhysSelectionGetter;

    std::function<void()> m_OnProcessingCompleted;

    ProcessingSettings m_ProcessingSettings{};

    struct
    {
        struct
        {
            gl::c_Shader solidColor;
            gl::c_Shader monoOutput;
            gl::c_Shader monoOutputCubic;
            gl::c_Shader selectionOutline;
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
            gl::c_Shader vertex;
            gl::c_Shader passthrough;
        } vert;
    } m_GLShaders;

    struct
    {
        gl::c_Program copy;
        gl::c_Program monoOutput;
        gl::c_Program monoOutputCubic;
        gl::c_Program selectionOutline;
        gl::c_Program toneCurve;
        gl::c_Program gaussianHorz;
        gl::c_Program gaussianVert;
        gl::c_Program unsharpMask;
        gl::c_Program multiply;
        gl::c_Program divide;
    } m_GLPrograms;

    struct
    {
        gl::c_Buffer wholeImgScaled; ///< Whole image rectangle (with scaling applied).
        gl::c_Buffer selectionScaled; ///< Selection outline (with scaling applied); changes when new selection is being made.
        gl::c_Buffer lastChosenSelectionScaled; ///< Selection outline (with scaling applied); corresponds to `m_Selection * m_ZoomFactor`.
        gl::c_Buffer wholeSelection; ///< Vertices specify a full-screen quad, tex. coords correspond to `m_Selection`.
        gl::c_Buffer wholeSelectionAtZero; ///< Vertices specify a full-screen quad, tex. coords correspond to `m_Selection` positioned at (0, 0).
    } m_VBOs;

    /// All textures are GL_TEXTURE_RECTANGLE, GL_RED, GL_FLOAT (single-component 32-bit floating-point).
    struct
    {
        gl::c_Texture originalImg;

        // Textures below have the same size as `m_Selection`.

        gl::c_Texture aux;

        gl::c_Texture lrSharpened; ///< Result of L-R deconvolution sharpening of the original image.
        gl::c_Texture gaussianBlur; ///< Result of Gaussian blur of the sharpened image.
        gl::c_Texture unsharpMask; ///< Result of sharpening and unsharp masking.
        gl::c_Texture toneCurve; ///< Result of sharpening, unsharp masking and tone mapping.

        /// Lucy-Richardson deconvolution intermediate data.
        struct
        {
            gl::c_Texture original;
            gl::c_Texture buf1;
            gl::c_Texture buf2;
            gl::c_Texture estimateConvolved;
            gl::c_Texture convolvedDiv;
            gl::c_Texture convolved2;
        } LR;
    } m_Textures;

    /// Each FBO renders into the same-named texture from `m_Textures`.
    struct
    {
        gl::c_Framebuffer gaussianBlur;
        gl::c_Framebuffer aux;
        gl::c_Framebuffer lrSharpened;
        gl::c_Framebuffer toneCurve;
        gl::c_Framebuffer unsharpMask;

        struct
        {
            gl::c_Framebuffer original;
            gl::c_Framebuffer buf1;
            gl::c_Framebuffer original_buf1; ///< Special case: renders to both `original` and `buf1`.
            gl::c_Framebuffer buf2;
            gl::c_Framebuffer estimateConvolved;
            gl::c_Framebuffer convolvedDiv;
            gl::c_Framebuffer convolved2;
        } LR;
    } m_FBOs;


    /// Indicates if all OpenGL commands required for the processing step have been submitted for execution;
    /// if `true`, any new commands can rely on `m_Textures` containing valid output.
    struct
    {
        bool sharpening{false};
        bool unshMask{false};
        bool toneCurve{false};
    } m_ProcessingOutputValid;

    ScalingMethod m_ScalingMethod{ScalingMethod::LINEAR};

    std::vector<float> m_LRGaussian; // element [0] = max value
    std::vector<float> m_UnshMaskGaussian; // element [0] = max value

    c_OpenGLBackEnd(c_ScrolledView& imgView);

    void OnPaint(wxPaintEvent& event);

    /// Propagates `event` received by m_GLCanvas to `m_ImgView`.
    void PropagateEventToParent(wxMouseEvent& event);

    void MarkSelection();

    void FillWholeImgVBO();

    void FillLastChosenSelectionScaledVBO();

    void StartProcessing(ProcessingRequest procRequest);

    void StartLRDeconvolution();

    void StartUnsharpMasking();

    void StartToneMapping();

    void FillWholeSelectionVBO();

    void RenderProcessingResults();

    /// Convolves `src` with `gaussian` and writes the output to `dest`.
    ///
    /// Does not bind VBOs nor activates vertex attribute pointers.
    ///
    void GaussianConvolution(gl::c_Texture& src, gl::c_Framebuffer& dest, const std::vector<float>& gaussian);

    /// Does not bind VBOs nor activates vertex attribute pointers.
    void MultiplyTextures(gl::c_Texture& tex1, gl::c_Texture& tex2, gl::c_Framebuffer& dest);

    /// Does not bind VBOs nor activates vertex attribute pointers.
    void DivideTextures(gl::c_Texture& tex1, gl::c_Texture& tex2, gl::c_Framebuffer& dest);
};

} // namespace imppg::backend
