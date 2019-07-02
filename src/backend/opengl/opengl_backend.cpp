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
#include "../../gauss.h"
#include "../../logging.h"
#include "opengl_backend.h"

namespace imppg::backend
{

namespace uniforms
{
    const char* ScrollPos = "ScrollPos";
    const char* ViewportSize = "ViewportSize";
    const char* ZoomFactor = "ZoomFactor";
    const char* NumPoints = "NumPoints";
    const char* CurvePoints = "CurvePoints";
    const char* Splines = "Splines";
    const char* Image = "Image";
    const char* Smooth = "Smooth";
    const char* IsGamma = "IsGamma";
    const char* Gamma = "Gamma";
    const char* KernelRadius = "KernelRadius";
    const char* GaussianKernel = "GaussianKernel";
    const char* BlurredImage = "BlurredImage";
    const char* SelectionPos = "SelectionPos";
    const char* AmountMin = "AmountMin";
    const char* AmountMax = "AmountMax";
}

namespace attributes
{
    constexpr GLuint Position = 0;
    constexpr GLuint TexCoord = 1;
}

void c_OpenGLBackEnd::PropagateEventToParent(wxMouseEvent& event)
{
    event.ResumePropagation(1);
    event.Skip();
}

static const int IMPPG_GL_ATTRIBUTES[] =
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

static void SpecifyVertexAttribPointers()
{
    glVertexAttribPointer(
        attributes::Position,
        2,  // 2 components (x, y) per attribute value
        GL_FLOAT,
        GL_FALSE,
        4 * sizeof(GLfloat), // 4 components per vertex
        nullptr // attribute values at offset 0
    );

    glVertexAttribPointer(
        attributes::TexCoord,
        2,  // 2 components (x, y, tex.x, tex.y) per attribute value
        GL_FLOAT,
        GL_FALSE,
        4 * sizeof(GLfloat), // 4 components per vertex
        reinterpret_cast<const GLvoid*>(2 * sizeof(GLfloat)) // attribute values at offset 2
    );
}

void c_OpenGLBackEnd::CopyTextureFragment(gl::c_Texture& srcTex, gl::c_Framebuffer& destFBO, gl::c_Buffer& vbo)
{
    gl::c_FramebufferBinder binder(destFBO);
    m_GLPrograms.copy.Use();
    const int textureUnit = 0;
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_RECTANGLE, srcTex.Get());
    m_GLPrograms.copy.SetUniform1i(uniforms::Image, textureUnit);
    vbo.Bind();
    SpecifyVertexAttribPointers();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

std::unique_ptr<c_OpenGLBackEnd> c_OpenGLBackEnd::Create(c_ScrolledView& imgView)
{
    if (!wxGLCanvas::IsDisplaySupported(IMPPG_GL_ATTRIBUTES))
        return nullptr;
    else
        return std::unique_ptr<c_OpenGLBackEnd>(new c_OpenGLBackEnd(imgView));
}

void c_OpenGLBackEnd::OnIdle(wxIdleEvent& /*event*/)
{
    //event.RequestMore();
}

c_OpenGLBackEnd::c_OpenGLBackEnd(c_ScrolledView& imgView)
: m_ImgView(imgView)
{
    auto* sz = new wxBoxSizer(wxHORIZONTAL);
    m_GLCanvas = new wxGLCanvas(&imgView.GetContentsPanel(), wxID_ANY, IMPPG_GL_ATTRIBUTES);
    sz->Add(m_GLCanvas, 1, wxGROW);
    imgView.GetContentsPanel().SetSizer(sz);

    m_GLContext = new wxGLContext(m_GLCanvas);

    m_GLCanvas->Bind(wxEVT_SIZE, [this](wxSizeEvent&) { glViewport(0, 0, m_GLCanvas->GetSize().GetWidth(), m_GLCanvas->GetSize().GetHeight()); });

    m_GLCanvas->Bind(wxEVT_LEFT_DOWN,          &c_OpenGLBackEnd::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_MOTION,             &c_OpenGLBackEnd::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_LEFT_UP,            &c_OpenGLBackEnd::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_MOUSE_CAPTURE_LOST, [](wxMouseCaptureLostEvent& event) { event.ResumePropagation(1); event.Skip(); });
    m_GLCanvas->Bind(wxEVT_MIDDLE_DOWN,        &c_OpenGLBackEnd::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_RIGHT_DOWN,         &c_OpenGLBackEnd::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_MIDDLE_UP,          &c_OpenGLBackEnd::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_RIGHT_UP,           &c_OpenGLBackEnd::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_MOUSEWHEEL,         &c_OpenGLBackEnd::PropagateEventToParent, this);
}

bool c_OpenGLBackEnd::MainWindowShown()
{
    m_GLCanvas->SetCurrent(*m_GLContext);

    const GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        Log::Print("Failed to initialize GLEW.");
        return false;
    }

    m_GLShaders.frag.monoOutput = gl::c_Shader(GL_FRAGMENT_SHADER, "shaders/mono_output.frag");
    m_GLShaders.frag.monoOutput = gl::c_Shader(GL_FRAGMENT_SHADER, "shaders/mono_output.frag");
    m_GLShaders.frag.monoOutputCubic = gl::c_Shader(GL_FRAGMENT_SHADER, "shaders/mono_output_cubic.frag");
    m_GLShaders.frag.selectionOutline = gl::c_Shader(GL_FRAGMENT_SHADER, "shaders/selection_outline.frag");
    m_GLShaders.frag.copy = gl::c_Shader(GL_FRAGMENT_SHADER, "shaders/copy.frag");
    m_GLShaders.frag.toneCurve = gl::c_Shader(GL_FRAGMENT_SHADER, "shaders/tone_curve.frag");
    m_GLShaders.frag.gaussianHorz = gl::c_Shader(GL_FRAGMENT_SHADER, "shaders/gaussian_horz.frag");
    m_GLShaders.frag.gaussianVert = gl::c_Shader(GL_FRAGMENT_SHADER, "shaders/gaussian_vert.frag");
    m_GLShaders.frag.unsharpMask = gl::c_Shader(GL_FRAGMENT_SHADER, "shaders/unsharp_mask.frag");

    m_GLShaders.vert.vertex = gl::c_Shader(GL_VERTEX_SHADER, "shaders/vertex.vert");
    m_GLShaders.vert.copy = gl::c_Shader(GL_VERTEX_SHADER, "shaders/copy_texture.vert");

    m_GLPrograms.monoOutput = gl::c_Program(
        { &m_GLShaders.frag.monoOutput,
          &m_GLShaders.vert.vertex },
        { uniforms::Image,
          uniforms::ViewportSize,
          uniforms::ScrollPos },
        {}
    );

    m_GLPrograms.monoOutputCubic = gl::c_Program(
        { &m_GLShaders.frag.monoOutputCubic,
          &m_GLShaders.vert.vertex },
        { uniforms::Image,
          uniforms::ViewportSize,
          uniforms::ScrollPos },
        {}
    );

    m_GLPrograms.selectionOutline = gl::c_Program(
        { &m_GLShaders.frag.selectionOutline,
          &m_GLShaders.vert.vertex },
        { uniforms::ViewportSize,
          uniforms::ScrollPos },
        {}
    );

    m_GLPrograms.copy = gl::c_Program(
        { &m_GLShaders.frag.copy,
          &m_GLShaders.vert.copy },
        { uniforms::Image },
        {}
    );

    m_GLPrograms.toneCurve = gl::c_Program(
        { &m_GLShaders.frag.toneCurve,
          &m_GLShaders.vert.copy },
        { uniforms::Image,
          uniforms::NumPoints,
          uniforms::CurvePoints,
          uniforms::Splines,
          uniforms::Smooth,
          uniforms::IsGamma,
          uniforms::Gamma },
        {}
    );

    m_GLPrograms.gaussianHorz = gl::c_Program(
        { &m_GLShaders.frag.gaussianHorz,
          &m_GLShaders.vert.copy },
        { uniforms::Image,
          uniforms::KernelRadius,
          uniforms::GaussianKernel },
        {}
    );

    m_GLPrograms.gaussianVert = gl::c_Program(
        { &m_GLShaders.frag.gaussianVert,
          &m_GLShaders.vert.copy },
        { uniforms::Image,
          uniforms::KernelRadius,
          uniforms::GaussianKernel },
        {}
    );

    m_GLPrograms.unsharpMask = gl::c_Program(
        { &m_GLShaders.frag.unsharpMask,
          &m_GLShaders.vert.copy },
        { uniforms::Image,
          uniforms::BlurredImage,
          uniforms::SelectionPos,
          uniforms::AmountMax },
        {}
    );

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

    m_VBOs.wholeImgScaled = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);
    m_VBOs.selectionScaled = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);
    m_VBOs.wholeSelection = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);
    m_VBOs.wholeSelectionAtZero = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);
    m_VBOs.lastChosenSelectionScaled = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glEnableVertexAttribArray(attributes::Position);
    glEnableVertexAttribArray(attributes::TexCoord);

    return true;
}

void c_OpenGLBackEnd::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(m_GLCanvas);
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_Img.has_value())
    {
        auto& prog = (m_ScalingMethod == ScalingMethod::CUBIC) ? m_GLPrograms.monoOutputCubic : m_GLPrograms.monoOutput;
        prog.Use();

        const int textureUnit = 0;
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_RECTANGLE, m_Textures.originalImg.Get());
        prog.SetUniform1i(uniforms::Image, textureUnit);
        const auto scrollpos = m_ImgView.GetScrollPos();
        prog.SetUniform2i(uniforms::ScrollPos, scrollpos.x, scrollpos.y);
        prog.SetUniform2i(
            uniforms::ViewportSize,
            m_GLCanvas->GetSize().GetWidth(),
            m_GLCanvas->GetSize().GetHeight()
        );

        m_VBOs.wholeImgScaled.Bind();
        SpecifyVertexAttribPointers();
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        RenderProcessingResults();

        MarkSelection();
    }

    m_GLCanvas->SwapBuffers();
}

void c_OpenGLBackEnd::RenderProcessingResults()
{
    auto& prog = (m_ScalingMethod == ScalingMethod::CUBIC) ? m_GLPrograms.monoOutputCubic : m_GLPrograms.monoOutput;
    prog.Use();

    const int textureUnit = 0;
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_RECTANGLE, m_Textures.toneCurve.Get());
    prog.SetUniform1i(uniforms::Image, textureUnit);
    const auto scrollpos = m_ImgView.GetScrollPos();
    prog.SetUniform2i(uniforms::ScrollPos, scrollpos.x, scrollpos.y);
    prog.SetUniform2i(
        uniforms::ViewportSize,
        m_GLCanvas->GetSize().GetWidth(),
        m_GLCanvas->GetSize().GetHeight()
    );

    m_VBOs.lastChosenSelectionScaled.Bind();
    SpecifyVertexAttribPointers();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void c_OpenGLBackEnd::MarkSelection()
{
    const wxRect physSelection = m_PhysSelectionGetter();
    const wxPoint scrollPos = m_ImgView.GetScrollPos();

    const GLfloat x0 = physSelection.x + scrollPos.x;
    const GLfloat y0 = physSelection.y + scrollPos.y;
    /// 4 values per vertex: position, texture coords (not used)
    const GLfloat vertexData[] =
    {
        x0, y0,                                              0, 0,
        x0 + physSelection.width, y0,                        0, 0,
        x0 + physSelection.width, y0 + physSelection.height, 0, 0,
        x0, y0 + physSelection.height,                       0, 0
    };
    m_VBOs.selectionScaled.SetData(vertexData, sizeof(vertexData), GL_DYNAMIC_DRAW);

    auto& prog = m_GLPrograms.selectionOutline;
    prog.Use();
    const auto scrollpos = m_ImgView.GetScrollPos();
    prog.SetUniform2i(uniforms::ScrollPos, scrollpos.x, scrollpos.y);
    prog.SetUniform2i(
        uniforms::ViewportSize,
        m_GLCanvas->GetSize().GetWidth(),
        m_GLCanvas->GetSize().GetHeight()
    );

    m_VBOs.selectionScaled.Bind();
    SpecifyVertexAttribPointers();
    glDrawArrays(GL_LINE_LOOP, 0, 4);
}

void c_OpenGLBackEnd::ImageViewScrolledOrResized(float /*zoomFactor*/)
{
    m_GLCanvas->Refresh(false);
}

void c_OpenGLBackEnd::ImageViewZoomChanged(float zoomFactor)
{
    m_ZoomFactor = zoomFactor;
    FillWholeImgVBO();
    FillLastChosenSelectionScaledVBO();
}

void c_OpenGLBackEnd::FillWholeSelectionVBO()
{
    const GLfloat x0 = static_cast<GLfloat>(m_Selection.x);
    const GLfloat y0 = static_cast<GLfloat>(m_Selection.y);
    const GLfloat width = static_cast<GLfloat>(m_Selection.width);
    const GLfloat height = static_cast<GLfloat>(m_Selection.height);

    /// 4 values per vertex: position (a full-screen quad), texture coords
    const GLfloat vertexData[] = {
        -1.0f, -1.0f,    x0, y0,
        1.0f, -1.0f,     x0 + width, y0,
        1.0f, 1.0f,      x0 + width, y0 + height,
        -1.0f, 1.0f,     x0, y0 + height
    };
    m_VBOs.wholeSelection.SetData(vertexData, sizeof(vertexData), GL_DYNAMIC_DRAW);

    /// 4 values per vertex: position (a full-screen quad), texture coords
    const GLfloat vertexDataAtZero[] = {
        -1.0f, -1.0f,    0, 0,
        1.0f, -1.0f,     width, 0,
        1.0f, 1.0f,      width, height,
        -1.0f, 1.0f,     0, height
    };
    m_VBOs.wholeSelectionAtZero.SetData(vertexDataAtZero, sizeof(vertexDataAtZero), GL_DYNAMIC_DRAW);
}

void c_OpenGLBackEnd::FillLastChosenSelectionScaledVBO()
{
    const GLfloat sx0     = m_ZoomFactor * m_Selection.x;
    const GLfloat sy0     = m_ZoomFactor * m_Selection.y;
    const GLfloat width   = static_cast<GLfloat>(m_Selection.width);
    const GLfloat height  = static_cast<GLfloat>(m_Selection.height);
    const GLfloat swidth  = width * m_ZoomFactor;
    const GLfloat sheight = height * m_ZoomFactor;

    /// 4 values per vertex: position, texture coords
    const GLfloat vertexDataScaled[] = {
        sx0,          sy0,              0, 0,
        sx0 + swidth, sy0,              width, 0,
        sx0 + swidth, sy0 + sheight,    width, height,
        sx0,          sy0 + sheight,    0, height
    };
    m_VBOs.lastChosenSelectionScaled.SetData(vertexDataScaled, sizeof(vertexDataScaled), GL_DYNAMIC_DRAW);
}

void c_OpenGLBackEnd::FillWholeImgVBO()
{
    const GLfloat width =  static_cast<GLfloat>(m_Img.value().GetWidth());
    const GLfloat height = static_cast<GLfloat>(m_Img.value().GetHeight());
    const GLfloat swidth = width * m_ZoomFactor;
    const GLfloat sheight = height * m_ZoomFactor;

    /// 4 values per vertex: position, texture coords
    const GLfloat vertexData[] = {
        0.0f,   0.0f,       0.0f,  0.0f,
        swidth, 0.0f,       width, 0.0f,
        swidth, sheight,    width, height,
        0.0f,   sheight,    0.0f,  height
    };

    m_VBOs.wholeImgScaled.SetData(vertexData, sizeof(vertexData), GL_DYNAMIC_DRAW);
}

void c_OpenGLBackEnd::FileOpened(c_Image&& img, std::optional<wxRect> newSelection)
{
    m_Img = std::move(img);

    if (newSelection.has_value())
    {
        m_Selection = newSelection.value();
    }

    FillWholeImgVBO();
    FillWholeSelectionVBO();
    FillLastChosenSelectionScaledVBO();

    /// Only buffers with no row padding are currently accepted.
    ///
    /// To change it, use appropriate glPixelStore* calls in `c_Texture` constructor
    /// before calling `glTexImage2D`.
    IMPPG_ASSERT(m_Img.value().GetBuffer().GetBytesPerRow() == m_Img.value().GetWidth() * sizeof(float));

    m_Textures.originalImg = gl::c_Texture::CreateMono(
        m_Img.value().GetWidth(),
        m_Img.value().GetHeight(),
        m_Img.value().GetBuffer().GetRow(0),
        ScalingMethod::LINEAR == m_ScalingMethod
    );

    StartProcessing(ProcessingRequest::SHARPENING);
}

Histogram c_OpenGLBackEnd::GetHistogram()
{ 
    if (m_Img)
        return DetermineHistogram(m_Img.value(), m_Selection);
    else
        return Histogram{};
}

void c_OpenGLBackEnd::NewSelection(const wxRect& selection)
{
    m_Selection = selection;
    FillWholeSelectionVBO();
    FillLastChosenSelectionScaledVBO();
    StartProcessing(ProcessingRequest::SHARPENING);
}

void c_OpenGLBackEnd::StartProcessing(ProcessingRequest procRequest)
{
    // if the previous processing step(s) did not complete, we have to execute it (them) first
    if (procRequest == ProcessingRequest::TONE_CURVE && !m_ProcessingOutputValid.unshMask)
        procRequest = ProcessingRequest::UNSHARP_MASKING;

    if (procRequest == ProcessingRequest::UNSHARP_MASKING && !m_ProcessingOutputValid.sharpening)
        procRequest = ProcessingRequest::SHARPENING;

    switch (procRequest)
    {
    case ProcessingRequest::SHARPENING: // TESTING: fall-through
    case ProcessingRequest::UNSHARP_MASKING:
        StartUnsharpMasking();
        break;

    case ProcessingRequest::TONE_CURVE:
        StartToneMapping();
        break;

    case ProcessingRequest::NONE: IMPPG_ABORT();
    }
}

void c_OpenGLBackEnd::StartUnsharpMasking()
{
    m_ProcessingOutputValid.unshMask = false;
    m_ProcessingOutputValid.toneCurve = false;

    auto& tex = m_Textures.unsharpMask;
    if (!tex || tex.GetWidth() != m_Selection.width || tex.GetHeight() != m_Selection.height)
    {
        tex = gl::c_Texture::CreateMono(m_Selection.width, m_Selection.height, nullptr);
        m_FBOs.unsharpMask = gl::c_Framebuffer({ &tex });
    }

    auto& aux = m_Textures.aux;
    if (!aux || aux.GetWidth() != m_Selection.width || aux.GetHeight() != m_Selection.height)
    {
        aux = gl::c_Texture::CreateMono(m_Selection.width, m_Selection.height, nullptr);
        m_FBOs.aux = gl::c_Framebuffer({ &aux });
    }

    auto& blurred = m_Textures.gaussianBlur;
    if (!blurred || blurred.GetWidth() != m_Selection.width || blurred.GetHeight() != m_Selection.height)
    {
        blurred = gl::c_Texture::CreateMono(m_Selection.width, m_Selection.height, nullptr);
        m_FBOs.gaussianBlur = gl::c_Framebuffer({ &blurred });
    }

    // horizontal convolution: originalImg -> aux
    {
        gl::c_FramebufferBinder binder(m_FBOs.aux);
        m_VBOs.wholeSelection.Bind();
        SpecifyVertexAttribPointers();

        auto& prog = m_GLPrograms.gaussianHorz;
        prog.Use();
        const int textureUnit = 0;
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_RECTANGLE, m_Textures.originalImg.Get());
        prog.SetUniform1i(uniforms::Image, textureUnit);
        prog.SetUniform1i(uniforms::KernelRadius, m_UnshMaskGaussian.size());
        glUniform1fv(prog.GetUniform(uniforms::GaussianKernel), m_UnshMaskGaussian.size(), m_UnshMaskGaussian.data());

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    // vertical convolution: aux -> gaussianBlur
    {
        gl::c_FramebufferBinder binder(m_FBOs.gaussianBlur);
        m_VBOs.wholeSelectionAtZero.Bind();
        SpecifyVertexAttribPointers();

        auto& prog = m_GLPrograms.gaussianVert;
        prog.Use();
        const int textureUnit = 0;
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_RECTANGLE, m_Textures.aux.Get());
        prog.SetUniform1i(uniforms::Image, textureUnit);
        prog.SetUniform1i(uniforms::KernelRadius, m_UnshMaskGaussian.size());
        glUniform1fv(prog.GetUniform(uniforms::GaussianKernel), m_UnshMaskGaussian.size(), m_UnshMaskGaussian.data());

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    // apply the unsharp mask
    {
        gl::c_FramebufferBinder binder(m_FBOs.unsharpMask);
        m_VBOs.wholeSelection.Bind();
        SpecifyVertexAttribPointers();

        auto& prog = m_GLPrograms.unsharpMask;
        prog.Use();

        int textureUnit = 0;
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_RECTANGLE, m_Textures.originalImg.Get());
        prog.SetUniform1i(uniforms::Image, textureUnit);

        textureUnit++;
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_RECTANGLE, m_Textures.gaussianBlur.Get());
        prog.SetUniform1i(uniforms::BlurredImage, textureUnit);
        prog.SetUniform2f(uniforms::SelectionPos, static_cast<GLfloat>(m_Selection.x), static_cast<GLfloat>(m_Selection.y));
        prog.SetUniform1f(uniforms::AmountMax, m_ProcessingSettings.unsharpMasking.amountMax);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    m_ProcessingOutputValid.unshMask = true;

    StartProcessing(ProcessingRequest::TONE_CURVE);
}

void c_OpenGLBackEnd::StartToneMapping()
{
    m_ProcessingOutputValid.toneCurve = false;

    auto& tex = m_Textures.toneCurve;
    if (!tex || tex.GetWidth() != m_Selection.width || tex.GetHeight() != m_Selection.height)
    {
        tex = gl::c_Texture::CreateMono(m_Selection.width, m_Selection.height, nullptr, ScalingMethod::LINEAR == m_ScalingMethod);
        m_FBOs.toneCurve = gl::c_Framebuffer({ &tex });
    }

    gl::c_FramebufferBinder binder(m_FBOs.toneCurve);

    auto& prog = m_GLPrograms.toneCurve;
    prog.Use();
    const int textureUnit = 0;
    glActiveTexture(GL_TEXTURE0 + textureUnit);
    glBindTexture(GL_TEXTURE_RECTANGLE, m_Textures.unsharpMask.Get());
    const auto& tcurve = m_ProcessingSettings.toneCurve;
    prog.SetUniform1i(uniforms::Image, textureUnit);
    prog.SetUniform1i(uniforms::NumPoints, tcurve.GetNumPoints());
    prog.SetUniform1i(uniforms::Smooth, tcurve.GetSmooth());
    prog.SetUniform1i(uniforms::IsGamma, tcurve.IsGammaMode());
    prog.SetUniform1f(uniforms::Gamma, tcurve.GetGamma());
    glUniform2fv(prog.GetUniform(uniforms::CurvePoints), tcurve.GetNumPoints(), reinterpret_cast<const GLfloat*>(tcurve.GetPoints().data()));
    glUniform4fv(prog.GetUniform(uniforms::Splines), tcurve.GetNumPoints() - 1, reinterpret_cast<const GLfloat*>(tcurve.GetSplines().data()));

    m_VBOs.wholeSelectionAtZero.Bind();
    SpecifyVertexAttribPointers();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    m_ProcessingOutputValid.toneCurve = true;

    m_GLCanvas->Refresh(false);
    m_GLCanvas->Update();
}

void c_OpenGLBackEnd::SetScalingMethod(ScalingMethod scalingMethod)
{
    m_ScalingMethod = scalingMethod;
    bool linearInterpolation = (ScalingMethod::LINEAR == m_ScalingMethod);
    m_Textures.originalImg.SetLinearInterpolation(linearInterpolation);
    m_Textures.toneCurve.SetLinearInterpolation(linearInterpolation);
    m_GLCanvas->Refresh();
    m_GLCanvas->Update();
}

void c_OpenGLBackEnd::ToneCurveChanged(const ProcessingSettings& procSettings)
{
    m_ProcessingSettings.toneCurve = procSettings.toneCurve;
    StartToneMapping();
}

void c_OpenGLBackEnd::UnshMaskSettingsChanged(const ProcessingSettings& procSettings)
{
    const bool recalculateUnshMaskGaussian = (m_ProcessingSettings.unsharpMasking.sigma != procSettings.unsharpMasking.sigma);
    m_ProcessingSettings.unsharpMasking = procSettings.unsharpMasking;
    if (recalculateUnshMaskGaussian)
    {
        m_UnshMaskGaussian = CalculateHalf1DGaussianKernel(
            static_cast<int>(ceilf(m_ProcessingSettings.unsharpMasking.sigma) * 3),
            m_ProcessingSettings.unsharpMasking.sigma
        );
    }

    StartProcessing(ProcessingRequest::UNSHARP_MASKING);
}

void c_OpenGLBackEnd::NewProcessingSettings(const ProcessingSettings& procSettings)
{
    const bool recalculateLRGaussian = (m_ProcessingSettings.LucyRichardson.sigma != procSettings.LucyRichardson.sigma);
    const bool recalculateUnshMaskGaussian = (m_ProcessingSettings.unsharpMasking.sigma != procSettings.unsharpMasking.sigma);

    m_ProcessingSettings = procSettings;

    if (recalculateLRGaussian)
        m_LRGaussian = CalculateHalf1DGaussianKernel(
            static_cast<int>(ceilf(m_ProcessingSettings.LucyRichardson.sigma) * 3),
            m_ProcessingSettings.LucyRichardson.sigma
        );

    if (recalculateUnshMaskGaussian)
        m_UnshMaskGaussian = CalculateHalf1DGaussianKernel(
            static_cast<int>(ceilf(m_ProcessingSettings.unsharpMasking.sigma) * 3),
            m_ProcessingSettings.unsharpMasking.sigma
        );

    if (m_Img)
        StartProcessing(ProcessingRequest::SHARPENING);
}

} // namespace imppg::backend
