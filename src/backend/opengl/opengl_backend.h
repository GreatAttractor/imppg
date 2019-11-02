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

#include "common.h"
#include "scrolled_view.h"
#include "backend/backend.h"
#include "backend/opengl/gl_utils.h"

#include <wx/glcanvas.h> // has to be below other OpenGL-related includes

namespace imppg::backend {

class c_OpenGLBackEnd: public IBackEnd
{
public:
    /// Returns `nullptr` if OpenGL cannot be initialized.
    static std::unique_ptr<c_OpenGLBackEnd> Create(c_ScrolledView& imgView);

    ~c_OpenGLBackEnd() override;

    void ImageViewScrolledOrResized(float zoomFactor) override;

    void ImageViewZoomChanged(float zoomFactor) override;

    void SetImage(c_Image&& img, std::optional<wxRect> newSelection) override;

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

    void SetScalingMethod(ScalingMethod scalingMethod) override;

    const std::optional<c_Image>& GetImage() const override { return m_Img; }

    void OnIdle(wxIdleEvent& event) override;

    void SetProgressTextHandler(std::function<void(wxString)> handler) override { m_ProgressTextHandler = handler; }


private:
    c_ScrolledView& m_ImgView;

    std::unique_ptr<wxGLCanvas> m_GLCanvas;

    std::unique_ptr<wxGLContext> m_GLContext;

    std::optional<c_Image> m_Img;

    float m_ZoomFactor{ZOOM_NONE};

    float m_NewZoomFactor{ZOOM_NONE};

    wxRect m_Selection; ///< Image fragment selected for processing (in logical image coords).

    /// Provides selection in physical image view coords.
    std::function<wxRect()> m_PhysSelectionGetter;

    std::function<void()> m_OnProcessingCompleted;

    std::function<void(wxString)> m_ProgressTextHandler;

    ProcessingSettings m_ProcessingSettings{};

    struct
    {
        struct
        {
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

    // Lucy-Richardson deconvolution work issuing and synchronization.
    struct
    {
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

    /// Reinitializes texture and FBO if their size differs from `size`.
    void InitTextureAndFBO(TexFbo& texFbo, const wxSize& size);

    /// Issues an OpenGL command batch performing L-R iterations.
    void IssueLRCommandBatch();
};

} // namespace imppg::backend
