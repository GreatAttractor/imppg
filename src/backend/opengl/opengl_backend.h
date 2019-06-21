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

    void FileOpened(c_Image&& img) override;

    Histogram GetHistogram() override;

    void SetPhysicalSelectionGetter(std::function<wxRect()> getter) override { m_PhysSelectionGetter = getter; }

    void SetProcessingCompletedHandler(std::function<void()> handler) override { m_OnProcessingCompleted = handler; }

    void NewSelection(const wxRect& selection) override;

    bool MainWindowShown() override;

    void RefreshRect(const wxRect&) override { m_ImgView.GetContentsPanel().Refresh(false); }

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

    c_OpenGLBackEnd(c_ScrolledView& imgView);

    void OnPaint(wxPaintEvent& event);

    /// Propagates `event` received by m_GLCanvas to `m_ImgView`, taking scroll position into account.
    void PropagateEventToParent(wxMouseEvent& event);

    void MarkSelection();

    void FillWholeImgVBO();

    struct
    {
        gl::c_Shader solidColor;
        gl::c_Shader unprocessedImg;
        gl::c_Shader selectionOutline;

        gl::c_Shader vertex;
    } m_GLShaders;

    struct
    {
        gl::c_Program unprocessedImg;
        gl::c_Program selectionOutline;
    } m_GLPrograms;

    struct
    {
        gl::c_Buffer wholeImg;
        gl::c_Buffer selectionScaled; ///< Selection outline (with scaling applied).
    } m_VBOs;

    struct
    {
        gl::c_Texture originalImg;
    } m_Textures;
};

} // namespace imppg::backend
