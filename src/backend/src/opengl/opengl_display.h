/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2021 Filip Szczerek <ga.software@yahoo.com>

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
    OpenGL display back end declaration.
*/

#ifndef IMPPG_GL_DISPLAY_BACKEND_H
#define IMPPG_GL_DISPLAY_BACKEND_H

#include <functional>
#include <optional>
#include <memory>
#include <wx/bitmap.h>
#include <wx/event.h>

#include "common/scrolled_view.h"
#include "backend/backend.h"
#include "opengl/opengl_proc.h"
#include "opengl/gl_utils.h"

#include <wx/glcanvas.h> // has to be below other OpenGL-related includes

namespace imppg::backend {

class c_OpenGLDisplay: public IDisplayBackEnd
{
public:
    c_OpenGLDisplay(c_ScrolledView& imgView, unsigned lRCmdBatchSizeMpixIters);

    ~c_OpenGLDisplay() override;

    void ImageViewScrolledOrResized(float zoomFactor) override;

    void ImageViewZoomChanged(float zoomFactor) override;

    void SetImage(c_Image&& img, std::optional<wxRect> newSelection) override;

    Histogram GetHistogram() override;

    void SetPhysicalSelectionGetter(std::function<wxRect()> getter) override { m_PhysSelectionGetter = getter; }

    void SetScaledLogicalSelectionGetter(std::function<wxRect()>) override {}

    void SetProcessingCompletedHandler(std::function<void(CompletionStatus)> handler) override { m_OnProcessingCompleted = handler; }

    void NewSelection(const wxRect& selection, const wxRect& prevScaledLogicalSelection) override;

    void RefreshRect(const wxRect&) override { m_ImgView.GetContentsPanel().Refresh(false); }

    void NewProcessingSettings(const ProcessingSettings& procSettings) override;

    void LRSettingsChanged(const ProcessingSettings& procSettings) override;

    void UnshMaskSettingsChanged(const ProcessingSettings& procSettings) override;

    void ToneCurveChanged(const ProcessingSettings& procSettings) override;

    void SetScalingMethod(ScalingMethod scalingMethod) override;

    const std::shared_ptr<const c_Image>& GetImage() const override { return m_Img; }

    void OnIdle(wxIdleEvent& event) override;

    void SetProgressTextHandler(std::function<void(wxString)> handler) override { m_ProgressTextHandler = handler; }

    c_Image GetProcessedSelection() override;

    bool ProcessingInProgress() override;

    void AbortProcessing() override;


private:
    std::unique_ptr<c_OpenGLProcessing> m_Processor;

    c_ScrolledView& m_ImgView;

    std::unique_ptr<wxGLCanvas> m_GLCanvas;

    std::unique_ptr<wxGLContext> m_GLContext;

    std::shared_ptr<const c_Image> m_Img;

    float m_ZoomFactor{ZOOM_NONE};

    /// On some platforms (wxGTK3, wxOSX) GLCanvas is affected by screen scaling.
    double m_GLCanvasScaleFactor{1.0};

    wxRect m_Selection; ///< Image fragment selected for processing (in logical image coords).

    /// Provides selection in physical image view coords.
    std::function<wxRect()> m_PhysSelectionGetter;

    std::function<void(CompletionStatus)> m_OnProcessingCompleted;

    std::function<void(wxString)> m_ProgressTextHandler;

    struct
    {
        struct
        {
            gl::c_Shader monoOutput;
            gl::c_Shader monoOutputCubic;
            gl::c_Shader selectionOutline;
        } frag;

        struct
        {
            gl::c_Shader vertex;
        } vert;
    } m_GLShaders;

    struct
    {
        gl::c_Program monoOutput;
        gl::c_Program monoOutputCubic;
        gl::c_Program selectionOutline;
    } m_GLPrograms;

    struct
    {
        gl::c_Buffer wholeImgScaled; ///< Whole image rectangle (with scaling applied).
        gl::c_Buffer selectionScaled; ///< Selection outline (with scaling applied); changes when new selection is being made.
        gl::c_Buffer lastChosenSelectionScaled; ///< Selection outline (with scaling applied); corresponds to `m_Selection * m_ZoomFactor`.
    } m_VBOs;

    ScalingMethod m_ScalingMethod{ScalingMethod::LINEAR};

    bool m_DeferredCompletionHandlerCall = false;

    void OnPaint(wxPaintEvent& event);

    void MarkSelection();

    void FillWholeImgVBO();

    void FillLastChosenSelectionScaledVBO();

    void RenderProcessingResults();

    void SetGLContextOnGLCanvas(void);
};

} // namespace imppg::backend

#endif // IMPPG_GL_DISPLAY_BACKEND_H
