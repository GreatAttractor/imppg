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
    OpenGL display back end implementation.
*/

#include <GL/glew.h>
#include <utility>
#include <wx/dcclient.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/stdpaths.h>

#include "appconfig.h"
#include "backend/opengl/gl_common.h"
#include "backend/opengl/opengl_display.h"
#include "backend/opengl/uniforms.h"
#include "common.h"
#include "logging.h"

namespace imppg::backend
{

void c_OpenGLDisplay::PropagateEventToParent(wxMouseEvent& event)
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

std::unique_ptr<c_OpenGLDisplay> c_OpenGLDisplay::Create(c_ScrolledView& imgView)
{
    if (!wxGLCanvas::IsDisplaySupported(IMPPG_GL_ATTRIBUTES))
        return nullptr;
    else
        return std::unique_ptr<c_OpenGLDisplay>(new c_OpenGLDisplay(imgView));
}

c_OpenGLDisplay::c_OpenGLDisplay(c_ScrolledView& imgView)
: m_ImgView(imgView)
{
    auto* sz = new wxBoxSizer(wxHORIZONTAL);
    m_GLCanvas = std::make_unique<wxGLCanvas>(&imgView.GetContentsPanel(), wxID_ANY, IMPPG_GL_ATTRIBUTES);
    sz->Add(m_GLCanvas.get(), 1, wxGROW);
    imgView.GetContentsPanel().SetSizer(sz);

    m_GLContext = std::make_unique<wxGLContext>(m_GLCanvas.get());

    m_GLCanvas->Bind(wxEVT_SIZE, [this](wxSizeEvent& event) {
        const auto s = event.GetSize();
        glViewport(0, 0, s.GetWidth(), s.GetHeight());
        event.Skip();
    });

    m_GLCanvas->Bind(wxEVT_LEFT_DOWN,          &c_OpenGLDisplay::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_MOTION,             &c_OpenGLDisplay::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_LEFT_UP,            &c_OpenGLDisplay::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_MOUSE_CAPTURE_LOST, [](wxMouseCaptureLostEvent& event) { event.ResumePropagation(1); event.Skip(); });
    m_GLCanvas->Bind(wxEVT_MIDDLE_DOWN,        &c_OpenGLDisplay::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_RIGHT_DOWN,         &c_OpenGLDisplay::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_MIDDLE_UP,          &c_OpenGLDisplay::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_RIGHT_UP,           &c_OpenGLDisplay::PropagateEventToParent, this);
    m_GLCanvas->Bind(wxEVT_MOUSEWHEEL,         &c_OpenGLDisplay::PropagateEventToParent, this);
}

bool c_OpenGLDisplay::MainWindowShown()
{
    if (!m_GLCanvas->SetCurrent(*m_GLContext))
    {
        Log::Print("Failed to make GL context current.");
        return false;
    }

    const GLenum err = glewInit();
    if (GLEW_OK != err)
    {
        Log::Print("Failed to initialize GLEW.");
        return false;
    }

    auto shaderDir = wxFileName(wxStandardPaths::Get().GetExecutablePath());
    shaderDir.AppendDir("shaders");
    if (!shaderDir.Exists())
    {
        shaderDir.AssignCwd();
        shaderDir.AppendDir("shaders");
    }

    m_GLShaders.frag.monoOutput       = gl::c_Shader(GL_FRAGMENT_SHADER, FromDir(shaderDir, "mono_output.frag"));
    m_GLShaders.frag.monoOutputCubic  = gl::c_Shader(GL_FRAGMENT_SHADER, FromDir(shaderDir, "mono_output_cubic.frag"));
    m_GLShaders.frag.selectionOutline = gl::c_Shader(GL_FRAGMENT_SHADER, FromDir(shaderDir, "selection_outline.frag"));

    m_GLShaders.vert.vertex      = gl::c_Shader(GL_VERTEX_SHADER, FromDir(shaderDir, "vertex.vert"));

    m_GLPrograms.monoOutput = gl::c_Program(
        { &m_GLShaders.frag.monoOutput,
          &m_GLShaders.vert.vertex },
        { uniforms::Image,
          uniforms::ViewportSize,
          uniforms::ScrollPos },
        {},
        "monoOutput"
    );

    m_GLPrograms.monoOutputCubic = gl::c_Program(
        { &m_GLShaders.frag.monoOutputCubic,
          &m_GLShaders.vert.vertex },
        { uniforms::Image,
          uniforms::ViewportSize,
          uniforms::ScrollPos },
        {},
        "monoOutputCubic"
    );

    m_GLPrograms.selectionOutline = gl::c_Program(
        { &m_GLShaders.frag.selectionOutline,
          &m_GLShaders.vert.vertex },
        { uniforms::ViewportSize,
          uniforms::ScrollPos },
        {},
        "selectionOutline"
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

    m_GLCanvas->Bind(wxEVT_PAINT, &c_OpenGLDisplay::OnPaint, this);

    m_VBOs.wholeImgScaled = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);
    m_VBOs.selectionScaled = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);
    m_VBOs.lastChosenSelectionScaled = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);

    gl::InitVAO();

    m_ImgView.Layout();
    //m_GLCanvas->GetEventHandler()->QueueEvent(new wxSizeEvent());

    m_Processor = std::make_unique<c_OpenGLProcessing>();

    m_Processor->SetProcessingCompletedHandler([this](CompletionStatus status) {
        if (status == CompletionStatus::COMPLETED)
        {
            m_GLCanvas->Refresh(false);
            m_GLCanvas->Update();

            // Something is broken with OpenGL (Fedora29 + GTK3): if there is a file save scheduled, calling m_OnProcessingCompleted()
            // will immediately cause a FileSave dialog to show. This somehow prevents proper filling of `m_TexFBOs.toneCurve.tex`
            // (it will contain garbage). Even glFlush() + glFinish() called here do not help.
            // Workaround: just set a flag here and act on it a moment later, in OnIdle().

            m_DeferredCompletionHandlerCall = true;
        }
    });

    m_Processor->SetProgressTextHandler([this](wxString info) { if (m_ProgressTextHandler) { m_ProgressTextHandler(info); } });

    return true;
}

void c_OpenGLDisplay::OnPaint(wxPaintEvent&)
{
    wxPaintDC dc(m_GLCanvas.get());

    glClear(GL_COLOR_BUFFER_BIT);

    if (m_Img.has_value())
    {
        auto& prog = (m_ScalingMethod == ScalingMethod::CUBIC) ? m_GLPrograms.monoOutputCubic : m_GLPrograms.monoOutput;
        prog.Use();

        gl::BindProgramTextures(prog, { {&m_Processor->GetOriginalImg(), uniforms::Image} });

        const auto scrollpos = m_ImgView.GetScrollPos();
        prog.SetUniform2i(uniforms::ScrollPos, scrollpos.x, scrollpos.y);
        prog.SetUniform2i(
            uniforms::ViewportSize,
            m_GLCanvas->GetSize().GetWidth(),
            m_GLCanvas->GetSize().GetHeight()
        );

        m_VBOs.wholeImgScaled.Bind();
        gl::SpecifyVertexAttribPointers();
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        RenderProcessingResults();
        MarkSelection();
    }

    m_GLCanvas->SwapBuffers();
}

void c_OpenGLDisplay::RenderProcessingResults()
{
    const gl::c_Texture* renderingResults = m_Processor->GetToneMappingOutput();
    if (!renderingResults) { return; }

    auto& prog = (m_ScalingMethod == ScalingMethod::CUBIC) ? m_GLPrograms.monoOutputCubic : m_GLPrograms.monoOutput;
    prog.Use();

    gl::BindProgramTextures(prog, { {renderingResults, uniforms::Image} });

    const auto scrollpos = m_ImgView.GetScrollPos();
    prog.SetUniform2i(uniforms::ScrollPos, scrollpos.x, scrollpos.y);
    prog.SetUniform2i(
        uniforms::ViewportSize,
        m_GLCanvas->GetSize().GetWidth(),
        m_GLCanvas->GetSize().GetHeight()
    );

    m_VBOs.lastChosenSelectionScaled.Bind();
    gl::SpecifyVertexAttribPointers();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void c_OpenGLDisplay::MarkSelection()
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
    gl::SpecifyVertexAttribPointers();
    glDrawArrays(GL_LINE_LOOP, 0, 4);
}

void c_OpenGLDisplay::ImageViewScrolledOrResized(float zoomFactor)
{
//    m_GLCanvas->Refresh(false);

    m_ZoomFactor = zoomFactor;
    if (m_Img.has_value())
    {
        FillWholeImgVBO();
        FillLastChosenSelectionScaledVBO();
    }

}

void c_OpenGLDisplay::ImageViewZoomChanged(float zoomFactor)
{
    m_ZoomFactor = zoomFactor;
    if (m_Img.has_value())
    {
        FillWholeImgVBO();
        FillLastChosenSelectionScaledVBO();
    }
}

void c_OpenGLDisplay::FillLastChosenSelectionScaledVBO()
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

void c_OpenGLDisplay::FillWholeImgVBO()
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

void c_OpenGLDisplay::SetImage(c_Image&& img, std::optional<wxRect> newSelection)
{
    if (newSelection.has_value())
    {
        m_Selection = newSelection.value();
        m_Processor->SetSelection(m_Selection);
    }

    m_Img = std::move(img);
    m_Processor->SetImage(m_Img.value(), ScalingMethod::LINEAR == m_ScalingMethod);

    FillWholeImgVBO();
    FillLastChosenSelectionScaledVBO();

    m_Processor->StartProcessing(ProcessingRequest::SHARPENING);
}

Histogram c_OpenGLDisplay::GetHistogram()
{
    if (const gl::c_Texture* unshMaskOutput = m_Processor->GetUsharpMaskOutput())
    {
        c_Image img(unshMaskOutput->GetWidth(), unshMaskOutput->GetHeight(), PixelFormat::PIX_MONO32F);
        glBindTexture(GL_TEXTURE_RECTANGLE, unshMaskOutput->Get());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glGetTexImage(GL_TEXTURE_RECTANGLE, 0, GL_RED, GL_FLOAT, img.GetRow(0));
        return DetermineHistogram(img, img.GetImageRect());
    }
    else if (m_Img)
        return DetermineHistogram(m_Img.value(), m_Selection);
    else
        return Histogram{};
}

void c_OpenGLDisplay::NewSelection(const wxRect& selection)
{
    m_Selection = selection;
    m_Processor->SetSelection(selection);
    FillLastChosenSelectionScaledVBO();
    if (m_Img.has_value())
    {
        m_Processor->StartProcessing(ProcessingRequest::SHARPENING);
    }
}

void c_OpenGLDisplay::SetScalingMethod(ScalingMethod scalingMethod)
{
    m_ScalingMethod = scalingMethod;
    bool linearInterpolation = (ScalingMethod::LINEAR == m_ScalingMethod);
    m_Processor->SetTexturesLinearInterpolation(linearInterpolation);
    m_GLCanvas->Refresh();
    m_GLCanvas->Update();
}

void c_OpenGLDisplay::ToneCurveChanged(const ProcessingSettings& procSettings)
{
    m_Processor->SetProcessingSettings(procSettings);
    if (m_Img.has_value())
    {
        m_Processor->StartProcessing(ProcessingRequest::TONE_CURVE);
    }
}

void c_OpenGLDisplay::UnshMaskSettingsChanged(const ProcessingSettings& procSettings)
{
    m_Processor->SetProcessingSettings(procSettings);
    if (m_Img.has_value())
    {
        m_Processor->StartProcessing(ProcessingRequest::UNSHARP_MASKING);
    }
}

void c_OpenGLDisplay::LRSettingsChanged(const ProcessingSettings& procSettings)
{
    m_Processor->SetProcessingSettings(procSettings);
    if (m_Img.has_value())
    {
        m_Processor->StartProcessing(ProcessingRequest::SHARPENING);
    }
}

void c_OpenGLDisplay::NewProcessingSettings(const ProcessingSettings& procSettings)
{
    m_Processor->SetProcessingSettings(procSettings);
    if (m_Img.has_value())
    {
        m_Processor->StartProcessing(ProcessingRequest::SHARPENING);
    }
}

c_OpenGLDisplay::~c_OpenGLDisplay()
{
    m_ImgView.GetContentsPanel().SetSizer(nullptr);
}

void c_OpenGLDisplay::OnIdle(wxIdleEvent& event)
{
    m_Processor->OnIdle(event);

    if (m_DeferredCompletionHandlerCall)
    {
        m_DeferredCompletionHandlerCall = false;
        if (m_OnProcessingCompleted)
        {
            m_OnProcessingCompleted(CompletionStatus::COMPLETED);
        }
    }
}

c_Image c_OpenGLDisplay::GetProcessedSelection()
{
    return m_Processor->GetProcessedSelection();
}

bool c_OpenGLDisplay::ProcessingInProgress()
{
    return m_Processor->IsProcessingInProgress();
}

void c_OpenGLDisplay::AbortProcessing()
{
    m_Processor->AbortProcessing();
}

} // namespace imppg::backend
