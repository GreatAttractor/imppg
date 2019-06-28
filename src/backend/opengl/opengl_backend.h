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

    void NewProcessingSettings(const ProcessingSettings& /*procSettings*/) override {}

    void LRSettingsChanged(const ProcessingSettings& /*procSettings*/) override {}

    void UnshMaskSettingsChanged(const ProcessingSettings& /*procSettings*/) override {}

    void ToneCurveChanged(const ProcessingSettings& /*procSettings*/) override {}

    void OnIdle(wxIdleEvent& event) override;

    void SetScalingMethod(ScalingMethod scalingMethod) override;

private:
    c_ScrolledView& m_ImgView;

    wxGLCanvas* m_GLCanvas{nullptr};

    wxGLContext* m_GLContext{nullptr};

    std::optional<c_Image> m_Img;

    float m_ZoomFactor{ZOOM_NONE};

    float m_NewZoomFactor{ZOOM_NONE};

    wxRect m_Selection; ///< Image fragment selected for processing (in logical image coords).

    /// Provides selection in physical image view coords.
    std::function<wxRect()> m_PhysSelectionGetter;

    std::function<void()> m_OnProcessingCompleted;

    struct
    {
        struct
        {
            gl::c_Shader solidColor;
            gl::c_Shader monoOutput;
            gl::c_Shader monoOutputCubic;
            gl::c_Shader selectionOutline;
            gl::c_Shader copy;
        } frag;

        struct
        {
            gl::c_Shader vertex;
            gl::c_Shader copy;
        } vert;
    } m_GLShaders;

    struct
    {
        gl::c_Program monoOutput;
        gl::c_Program monoOutputCubic;
        gl::c_Program selectionOutline;
        gl::c_Program copy;
    } m_GLPrograms;

    struct
    {
        gl::c_Buffer wholeImgScaled; ///< Whole image rectangle (with scaling applied).
        gl::c_Buffer selectionScaled; ///< Selection outline (with scaling applied); changes when new selection is being made.
        gl::c_Buffer lastChosenSelectionScaled; ///< Selection outline (with scaling applied); corresponds to `m_Selection * m_ZoomFactor`.
        gl::c_Buffer wholeSelection; ///< Vertices specify a full-screen quad, tex. coords correspond to `m_Selection`.
    } m_VBOs;

    struct
    {
        gl::c_Texture originalImg;

        // Textures below have the same size as `m_Selection`.
        gl::c_Texture toneCurve; ///< Contains results of sharpening, unsharp masking and tone mapping.
    } m_Textures;

    struct
    {
        gl::c_Framebuffer toneCurve; ///< FBO to render into `m_Textures.toneCurve`.
    } m_FBOs;


    /// Indicates if a processing step has been completed and corresponding texture
    /// in `m_Textures` contains valid output.
    struct
    {
        bool sharpening{false};
        bool unshMask{false};
        bool toneCurve{false};
    } m_ProcessingOutputValid;

    ScalingMethod m_ScalingMethod{ScalingMethod::LINEAR};

    c_OpenGLBackEnd(c_ScrolledView& imgView);

    void OnPaint(wxPaintEvent& event);

    /// Propagates `event` received by m_GLCanvas to `m_ImgView`.
    void PropagateEventToParent(wxMouseEvent& event);

    void MarkSelection();

    void FillWholeImgVBO();

    void FillLastChosenSelectionScaledVBO();

    void StartProcessing(ProcessingRequest procRequest);

    void StartToneMapping();

    void FillWholeSelectionVBO();

    void RenderProcessingResults();

    /// Reads a fragment of `srcTex` given by `vbo` and writes it to `destFBO` (and texture(s) attached to it).
    void CopyTextureFragment(gl::c_Texture& srcTex, gl::c_Framebuffer& destFBO, gl::c_Buffer& vbo);
};

} // namespace imppg::backend
