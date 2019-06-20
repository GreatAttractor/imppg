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

#include "../../common.h"
#include "opengl_backend.h"

namespace imppg::backend
{

namespace uniforms
{
    const char* ScrollPos = "ScrollPos";
    const char* ImageSize = "ImageSize";
    const char* ZoomFactor = "ZoomFactor";

    const char* Image = "Image";
}

void c_OpenGLBackEnd::PropagateEventToParentUnscrolled(wxMouseEvent& event)
{
    const auto viewPos = m_ImgView.GetScrollPos();
    const auto eventPos = event.GetPosition();
    event.SetPosition({ eventPos.x - viewPos.x, eventPos.y - viewPos.y });
    event.ResumePropagation(1);
    event.Skip();
}

c_OpenGLBackEnd::c_OpenGLBackEnd(c_ScrolledView& imgView)
: m_ImgView(imgView)
{
    const int glAttributes[] =
    {
        WX_GL_CORE_PROFILE,
        WX_GL_MAJOR_VERSION, 3,
        WX_GL_MINOR_VERSION, 3,
        // Supposedly the ones below are enabled by default,
        // but not for Intel HD Graphics 5500 + Mesa DRI + wxWidgets 3.0.4 (Fedora 29).
        WX_GL_RGBA,
        WX_GL_DOUBLEBUFFER,
        0
    };
    m_GLCanvas = new wxGLCanvas(&imgView.GetContentsPanel(), wxID_ANY, glAttributes);
    m_GLCanvas->SetSize({1, 1});
    m_GLContext = new wxGLContext(m_GLCanvas);

    m_GLCanvas->Bind(wxEVT_LEFT_DOWN,          &c_OpenGLBackEnd::PropagateEventToParentUnscrolled, this);
    m_GLCanvas->Bind(wxEVT_MOTION,             &c_OpenGLBackEnd::PropagateEventToParentUnscrolled, this);
    m_GLCanvas->Bind(wxEVT_LEFT_UP,            &c_OpenGLBackEnd::PropagateEventToParentUnscrolled, this);
    m_GLCanvas->Bind(wxEVT_MOUSE_CAPTURE_LOST, [](wxMouseCaptureLostEvent& event) { event.ResumePropagation(1); event.Skip(); });
    m_GLCanvas->Bind(wxEVT_MIDDLE_DOWN,        &c_OpenGLBackEnd::PropagateEventToParentUnscrolled, this);
    m_GLCanvas->Bind(wxEVT_RIGHT_DOWN,         &c_OpenGLBackEnd::PropagateEventToParentUnscrolled, this);
    m_GLCanvas->Bind(wxEVT_MIDDLE_UP,          &c_OpenGLBackEnd::PropagateEventToParentUnscrolled, this);
    m_GLCanvas->Bind(wxEVT_RIGHT_UP,           &c_OpenGLBackEnd::PropagateEventToParentUnscrolled, this);
    m_GLCanvas->Bind(wxEVT_MOUSEWHEEL,         &c_OpenGLBackEnd::PropagateEventToParentUnscrolled, this);
}

void c_OpenGLBackEnd::MainWindowShown()
{
    m_GLCanvas->SetCurrent(*m_GLContext);

    const GLenum err = glewInit();
    if (GLEW_OK != err)
        throw std::runtime_error("Failed to initialize GLEW");

    m_GLShaders.vertex = gl::c_Shader(GL_VERTEX_SHADER, "shaders/vertex.vert");
    m_GLShaders.unprocessedImg = gl::c_Shader(GL_FRAGMENT_SHADER, "shaders/unprocessed_image.frag");

    m_GLPrograms.unprocessedImg = gl::c_Program(
        { &m_GLShaders.unprocessedImg,
          &m_GLShaders.vertex },
        { uniforms::Image,
          uniforms::ImageSize,
          uniforms::ScrollPos,
          uniforms::ZoomFactor },
        {}
    );

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    const auto clearColor = m_ImgView.GetBackgroundColour();
    glClearColor(
        clearColor.Red()/255.0f,
        clearColor.Green()/255.0f,
        clearColor.Blue()/255.0f,
        1.0f
    );

    std::cout << glGetString(GL_VERSION) << std::endl;
    std::cout << glGetString(GL_RENDERER) << std::endl;

    m_GLCanvas->Bind(wxEVT_PAINT, &c_OpenGLBackEnd::OnPaint, this);
}

void c_OpenGLBackEnd::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(m_GLCanvas);
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_Img.has_value())
    {
        auto& prog = m_GLPrograms.unprocessedImg;
        prog.Use();

        const int textureUnit = 0;
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_RECTANGLE, m_Textures.originalImg.Get());
        prog.SetUniform1i(uniforms::Image, textureUnit);
        const auto scrollpos = m_ImgView.GetScrollPos();
        prog.SetUniform2i(uniforms::ScrollPos, scrollpos.x, scrollpos.y);
        prog.SetUniform2i(uniforms::ImageSize, m_Img.value().GetWidth(), m_Img.value().GetHeight());
        prog.SetUniform1f(uniforms::ZoomFactor, m_ZoomFactor);

        m_VBOs.wholeImg.Bind();
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

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
    const auto width = m_Img.value().GetWidth();
    const auto height = m_Img.value().GetHeight();
    const wxSize newSize{static_cast<int>(width * m_ZoomFactor),
                         static_cast<int>(height * m_ZoomFactor)};
    m_GLCanvas->SetSize(newSize);
    glViewport(0, 0, newSize.x, newSize.y);

    const GLfloat vertexData[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f,
    };

    m_VBOs.wholeImg = gl::c_Buffer(
        GL_ARRAY_BUFFER,
        vertexData,
        sizeof(vertexData),
        GL_STATIC_DRAW
    );

    constexpr GLuint vertPosAttrib = 0; // 0 corresponds to "location = 0" for attribute `Position` in shaders/vertex.vert
    glVertexAttribPointer(
        vertPosAttrib,
        2,  // 2 components (x, y) per attribute value
        GL_FLOAT,
        GL_FALSE,
        0,
        nullptr
    );
    glEnableVertexAttribArray(vertPosAttrib);

    m_Textures.originalImg = gl::c_Texture(
        GL_R32F,
        m_Img.value().GetWidth(), 
        m_Img.value().GetHeight(),
        GL_RED,
        GL_FLOAT,
        m_Img.value().GetBuffer().GetRow(0),
        false
    );
    IMPPG_ASSERT(m_Textures.originalImg);
}

Histogram c_OpenGLBackEnd::GetHistogram() { return Histogram{}; }

void c_OpenGLBackEnd::NewSelection(const wxRect& /*selection*/) {}


} // namespace imppg::backend
