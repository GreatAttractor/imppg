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
    OpenGL back end implementation.
*/

#include <GL/glew.h>
#include <wx/dcclient.h>
#include <wx/sizer.h>
#include "opengl_backend.h"

namespace imppg::backend
{

c_OpenGLBackEnd::c_OpenGLBackEnd(wxScrolledCanvas& imgView)
: m_ImgView(imgView)
{
    const int glAttributes[] =
    {
        WX_GL_CORE_PROFILE,
        WX_GL_MAJOR_VERSION, 3,
        WX_GL_MINOR_VERSION, 3,
        0
    };
    m_GLCanvas = new wxGLCanvas(&imgView, wxID_ANY, glAttributes);
    m_GLCanvas->SetSize({512, 512});

    m_GLContext = new wxGLContext(m_GLCanvas);
}

void c_OpenGLBackEnd::MainWindowShown()
{
    m_GLCanvas->SetCurrent(*m_GLContext);

    const GLenum err = glewInit();
    if (GLEW_OK != err)
        throw std::runtime_error("Failed to initialize GLEW");

    m_GLShaders.vertex = gl::c_Shader(GL_VERTEX_SHADER, "shaders/vertex.vert");
    m_GLShaders.solidColor = gl::c_Shader(GL_FRAGMENT_SHADER, "shaders/solid_color.frag");
    m_GLPrograms.solidColor = gl::c_Program({ &m_GLShaders.solidColor, &m_GLShaders.vertex }, {}, {});

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    IMPPG_ASSERT(gl::InitFullScreenQuad());

    constexpr GLuint vertAttrib = 0; // 0 corresponds to "location = 0" for attribute `Position` in shaders/vertex.vert
    glVertexAttribPointer(
        vertAttrib,
        2,  // 2 components (x, y) per attribute value
        GL_FLOAT,
        GL_FALSE,
        0,
        nullptr
    );
    glEnableVertexAttribArray(vertAttrib);

    glClearColor(0.66, 0.0, 0.0, 1.0);

    std::cout << glGetString(GL_VERSION) << std::endl;
    std::cout << glGetString(GL_RENDERER) << std::endl;

    m_GLCanvas->Bind(wxEVT_PAINT, &c_OpenGLBackEnd::OnPaint, this);
}

void c_OpenGLBackEnd::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(m_GLCanvas);
    glClear(GL_COLOR_BUFFER_BIT);

    m_GLPrograms.solidColor.Use();
    gl::DrawFullscreenQuad();

    m_GLCanvas->SwapBuffers();
}

void c_OpenGLBackEnd::ImageViewScrolledOrResized(float /*zoomFactor*/)
{
    m_GLCanvas->Refresh();
}

void c_OpenGLBackEnd::ImageViewZoomChanged(float zoomFactor)
{
    m_ZoomFactor = zoomFactor;
    const wxSize newSize{static_cast<int>(m_Img.value().GetWidth() * m_ZoomFactor),
                         static_cast<int>(m_Img.value().GetHeight() * m_ZoomFactor)};
    m_GLCanvas->SetSize(newSize);
    glViewport(0, 0, newSize.x, newSize.y);
}

void c_OpenGLBackEnd::FileOpened(c_Image&& img)
{
    m_Img = std::move(img);
    const wxSize newSize{static_cast<int>(m_Img.value().GetWidth() * m_ZoomFactor),
                         static_cast<int>(m_Img.value().GetHeight() * m_ZoomFactor)};
    m_GLCanvas->SetSize(newSize);
    glViewport(0, 0, newSize.x, newSize.y);
}

Histogram c_OpenGLBackEnd::GetHistogram() { return Histogram{}; }

void c_OpenGLBackEnd::NewSelection(const wxRect& /*selection*/) {}


} // namespace imppg::backend
