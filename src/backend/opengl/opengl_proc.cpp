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
    OpenGL processing back end implementation.
*/

#include "appconfig.h"
#include "backend/opengl/gl_common.h"
#include "backend/opengl/opengl_proc.h"
#include "backend/opengl/uniforms.h"
#include "gauss.h"
#include "imppg_assert.h"

#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace imppg::backend {

void c_OpenGLProcessing::StartProcessing(c_Image img, ProcessingSettings procSettings)
{
    (void)img; (void)procSettings;
    throw std::runtime_error("Not implemented yet!");
}

void c_OpenGLProcessing::SetProcessingCompletedHandler(std::function<void(CompletionStatus)> handler)
{
    m_ProcessingCompletedHandler = handler;
}

void c_OpenGLProcessing::SetProgressTextHandler(std::function<void(wxString)> handler)
{
    m_ProgressTextHandler = handler;
}

const c_Image& c_OpenGLProcessing::GetProcessedOutput()
{
    throw std::runtime_error("Not implemented yet!");
}

void c_OpenGLProcessing::OnIdle(wxIdleEvent& event)
{
    if (m_LRSync.abortRequested) { return; }

    if (m_Img && m_LRSync.numIterationsLeft > 0)
    {
        IssueLRCommandBatch();
    }

    if (m_LRSync.numIterationsLeft > 0)
    {
        event.RequestMore();
    }
}

void c_OpenGLProcessing::AbortProcessing()
{
    m_LRSync.abortRequested = true;
    if (m_ProcessingCompletedHandler)
    {
        m_ProcessingCompletedHandler(CompletionStatus::ABORTED);
    }
}

c_OpenGLProcessing::c_OpenGLProcessing()
{
    auto shaderDir = wxFileName(wxStandardPaths::Get().GetExecutablePath());
    shaderDir.AppendDir("shaders");
    if (!shaderDir.Exists())
    {
        shaderDir.AssignCwd();
        shaderDir.AppendDir("shaders");
    }

    m_GLShaders.frag.copy             = gl::c_Shader(GL_FRAGMENT_SHADER, FromDir(shaderDir, "copy.frag"));
    m_GLShaders.frag.toneCurve        = gl::c_Shader(GL_FRAGMENT_SHADER, FromDir(shaderDir, "tone_curve.frag"));
    m_GLShaders.frag.gaussianHorz     = gl::c_Shader(GL_FRAGMENT_SHADER, FromDir(shaderDir, "gaussian_horz.frag"));
    m_GLShaders.frag.gaussianVert     = gl::c_Shader(GL_FRAGMENT_SHADER, FromDir(shaderDir, "gaussian_vert.frag"));
    m_GLShaders.frag.unsharpMask      = gl::c_Shader(GL_FRAGMENT_SHADER, FromDir(shaderDir, "unsharp_mask.frag"));
    m_GLShaders.frag.divide           = gl::c_Shader(GL_FRAGMENT_SHADER, FromDir(shaderDir, "divide.frag"));
    m_GLShaders.frag.multiply         = gl::c_Shader(GL_FRAGMENT_SHADER, FromDir(shaderDir, "multiply.frag"));

    m_GLShaders.vert.passthrough = gl::c_Shader(GL_VERTEX_SHADER, FromDir(shaderDir, "pass-through.vert"));

    m_GLPrograms.copy = gl::c_Program(
        { &m_GLShaders.frag.copy,
          &m_GLShaders.vert.passthrough },
        { uniforms::Image },
        {}
    );

    m_GLPrograms.toneCurve = gl::c_Program(
        { &m_GLShaders.frag.toneCurve,
          &m_GLShaders.vert.passthrough },
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
          &m_GLShaders.vert.passthrough },
        { uniforms::Image,
          uniforms::KernelRadius,
          uniforms::GaussianKernel },
        {}
    );

    m_GLPrograms.gaussianVert = gl::c_Program(
        { &m_GLShaders.frag.gaussianVert,
          &m_GLShaders.vert.passthrough },
        { uniforms::Image,
          uniforms::KernelRadius,
          uniforms::GaussianKernel },
        {}
    );

    m_GLPrograms.unsharpMask = gl::c_Program(
        { &m_GLShaders.frag.unsharpMask,
          &m_GLShaders.vert.passthrough },
        { uniforms::Image,
          uniforms::BlurredImage,
          uniforms::AmountMax },
        {}
    );

    m_GLPrograms.multiply = gl::c_Program(
        { &m_GLShaders.frag.multiply,
          &m_GLShaders.vert.passthrough },
        { uniforms::InputArray1,
          uniforms::InputArray2 },
        {}
    );

    m_GLPrograms.divide = gl::c_Program(
        { &m_GLShaders.frag.divide,
          &m_GLShaders.vert.passthrough },
        { uniforms::InputArray1,
          uniforms::InputArray2 },
        {}
    );

    m_VBOs.wholeSelection = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);
    m_VBOs.wholeSelectionAtZero = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);
}

void c_OpenGLProcessing::StartProcessing(ProcessingRequest procRequest)
{
    // if the previous processing step(s) did not complete, we have to execute it (them) first
    if (procRequest == ProcessingRequest::TONE_CURVE && !m_ProcessingOutputValid.unshMask)
    {
        procRequest = ProcessingRequest::UNSHARP_MASKING;
    }

    if (procRequest == ProcessingRequest::UNSHARP_MASKING && !m_ProcessingOutputValid.sharpening)
    {
        procRequest = ProcessingRequest::SHARPENING;
    }

    switch (procRequest)
    {
    case ProcessingRequest::SHARPENING:
        StartLRDeconvolution();
        break;

    case ProcessingRequest::UNSHARP_MASKING:
        StartUnsharpMasking();
        break;

    case ProcessingRequest::TONE_CURVE:
        StartToneMapping();
        break;

    case ProcessingRequest::NONE: IMPPG_ABORT();
    }
}

void c_OpenGLProcessing::InitTextureAndFBO(c_OpenGLProcessing::TexFbo& texFbo, const wxSize& size)
{
    if (!texFbo.tex || texFbo.tex.GetWidth() != size.GetWidth() || texFbo.tex.GetHeight() != size.GetHeight())
    {
        texFbo.tex = gl::c_Texture::CreateMono(size.GetWidth(), size.GetHeight(), nullptr);
        texFbo.fbo = gl::c_Framebuffer({ &texFbo.tex });
    }
}

void c_OpenGLProcessing::IssueLRCommandBatch()
{
    if (m_LRSync.numIterationsLeft > 0)
    {
        if (!m_VBOs.wholeSelectionAtZero.IsBound())
        {
            m_VBOs.wholeSelectionAtZero.Bind();
            gl::SpecifyVertexAttribPointers();
        }

        const unsigned itersPerCmdBatch = std::max(1U, Configuration::LRCmdBatchSizeMpixIters * 1'000'000 / (m_Selection.width * m_Selection.height));
        const unsigned itersInThisBatch = std::min(itersPerCmdBatch, m_LRSync.numIterationsLeft);

        for (unsigned i = 0; i < itersInThisBatch; i++)
        {
            GaussianConvolution(m_LRSync.prev->tex, m_TexFBOs.LR.estimateConvolved.fbo, m_LRGaussian);
            DivideTextures(m_TexFBOs.LR.original.tex, m_TexFBOs.LR.estimateConvolved.tex, m_TexFBOs.LR.convolvedDiv.fbo);
            GaussianConvolution(m_TexFBOs.LR.convolvedDiv.tex, m_TexFBOs.LR.convolved2.fbo, m_LRGaussian);
            MultiplyTextures(m_LRSync.prev->tex, m_TexFBOs.LR.convolved2.tex, m_LRSync.next->fbo);
            std::swap(m_LRSync.prev, m_LRSync.next);
        }

        // We need to force the execution of the GL commands issued so far, so that the user can interrupt L-R processing via GUI
        // after each command batch.
        // Otherwise, the GL driver could defer and clump all the above commands for (time-consuming) execution together
        // (testing on my system showed it happens with the SwapBuffers() call).
        glFinish();

        m_LRSync.numIterationsLeft -= itersInThisBatch;
    }

    if (m_LRSync.numIterationsLeft == 0 && !m_ProcessingOutputValid.sharpening)
    {
        gl::c_FramebufferBinder binder(m_TexFBOs.lrSharpened.fbo);
        auto& prog = m_GLPrograms.copy;
        prog.Use();
        gl::BindProgramTextures(prog, { {&m_LRSync.next->tex, uniforms::Image} });
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        m_ProcessingOutputValid.sharpening = true;
        StartProcessing(ProcessingRequest::UNSHARP_MASKING);
    }
}

void c_OpenGLProcessing::StartLRDeconvolution()
{
    m_LRSync.abortRequested = false;

    if (m_ProgressTextHandler)
    {
        m_ProgressTextHandler(std::move(_(L"L\u2013R deconvolution...")));
    }

    m_ProcessingOutputValid.sharpening = false;
    m_ProcessingOutputValid.unshMask = false;
    m_ProcessingOutputValid.toneCurve = false;

    const wxSize ssize = m_Selection.GetSize();
    InitTextureAndFBO(m_TexFBOs.LR.original, ssize);
    InitTextureAndFBO(m_TexFBOs.LR.buf1, ssize);
    InitTextureAndFBO(m_TexFBOs.LR.buf2, ssize);
    InitTextureAndFBO(m_TexFBOs.LR.estimateConvolved, ssize);
    InitTextureAndFBO(m_TexFBOs.LR.convolvedDiv, ssize);
    InitTextureAndFBO(m_TexFBOs.LR.convolved2, ssize);
    InitTextureAndFBO(m_TexFBOs.lrSharpened, ssize);

    if (m_ProcessingSettings.LucyRichardson.iterations == 0)
    {
        m_VBOs.wholeSelection.Bind();
        gl::SpecifyVertexAttribPointers();
        auto& prog = m_GLPrograms.copy;
        prog.Use();
        gl::BindProgramTextures(prog, { {&m_OriginalImg, uniforms::Image} });
        gl::c_FramebufferBinder binder(m_TexFBOs.lrSharpened.fbo);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        m_ProcessingOutputValid.sharpening = true;
        StartProcessing(ProcessingRequest::UNSHARP_MASKING);
    }
    else
    {
        {
            m_VBOs.wholeSelection.Bind();
            gl::SpecifyVertexAttribPointers();
            auto& prog = m_GLPrograms.copy;
            prog.Use();

            gl::BindProgramTextures(prog, { {&m_OriginalImg, uniforms::Image} });

            // Under "AMD PITCAIRN (DRM 2.50.0, 5.1.16-200.fc29.x86_64, LLVM 7.0.1)" renderer (Radeon R370, Fedora 29) cannot attach
            // the textures `LR.original` and `LR.buf1` to a single FBO and just render once - only the first attachment gets filled.
            // So we have to render to each separately.
            {
                gl::c_FramebufferBinder binder(m_TexFBOs.LR.original.fbo);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
            {
                gl::c_FramebufferBinder binder(m_TexFBOs.LR.buf1.fbo);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
        }

        // `buf1` and `buf2` are used as ping-pong buffers
        m_LRSync.prev = &m_TexFBOs.LR.buf1;
        m_LRSync.next = &m_TexFBOs.LR.buf2;
        m_LRSync.numIterationsLeft = m_ProcessingSettings.LucyRichardson.iterations;

        IssueLRCommandBatch();
    }
}

void c_OpenGLProcessing::MultiplyTextures(gl::c_Texture& tex1, gl::c_Texture& tex2, gl::c_Framebuffer& dest)
{
    gl::c_FramebufferBinder binder(dest);
    auto& prog = m_GLPrograms.multiply;
    prog.Use();
    gl::BindProgramTextures(prog, { {&tex1, uniforms::InputArray1}, {&tex2, uniforms::InputArray2} });
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void c_OpenGLProcessing::DivideTextures(gl::c_Texture& tex1, gl::c_Texture& tex2, gl::c_Framebuffer& dest)
{
    gl::c_FramebufferBinder binder(dest);
    auto& prog = m_GLPrograms.divide;
    prog.Use();
    gl::BindProgramTextures(prog, { {&tex1, uniforms::InputArray1}, {&tex2, uniforms::InputArray2} });
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void c_OpenGLProcessing::GaussianConvolution(gl::c_Texture& src, gl::c_Framebuffer& dest, const std::vector<float>& gaussian)
{
    InitTextureAndFBO(m_TexFBOs.aux, wxSize{src.GetWidth(), src.GetHeight()});

    // horizontal convolution: src -> aux
    {
        gl::c_FramebufferBinder binder(m_TexFBOs.aux.fbo);
        auto& prog = m_GLPrograms.gaussianHorz;
        prog.Use();
        gl::BindProgramTextures(prog, { {&src, uniforms::Image} });
        prog.SetUniform1i(uniforms::KernelRadius, gaussian.size());
        glUniform1fv(prog.GetUniform(uniforms::GaussianKernel), gaussian.size(), gaussian.data());
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
    // vertical convolution: aux -> dest
    {
        gl::c_FramebufferBinder binder(dest);
        auto& prog = m_GLPrograms.gaussianVert;
        prog.Use();
        gl::BindProgramTextures(prog, { {&m_TexFBOs.aux.tex, uniforms::Image} });
        prog.SetUniform1i(uniforms::KernelRadius, gaussian.size());
        glUniform1fv(prog.GetUniform(uniforms::GaussianKernel), gaussian.size(), gaussian.data());
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
}


void c_OpenGLProcessing::StartUnsharpMasking()
{
    m_ProcessingOutputValid.unshMask = false;
    m_ProcessingOutputValid.toneCurve = false;

    InitTextureAndFBO(m_TexFBOs.unsharpMask, m_Selection.GetSize());
    InitTextureAndFBO(m_TexFBOs.gaussianBlur, m_Selection.GetSize());

    m_VBOs.wholeSelectionAtZero.Bind();
    gl::SpecifyVertexAttribPointers();
    GaussianConvolution(m_TexFBOs.lrSharpened.tex, m_TexFBOs.gaussianBlur.fbo, m_UnshMaskGaussian);

    // apply the unsharp mask
    {
        gl::c_FramebufferBinder binder(m_TexFBOs.unsharpMask.fbo);

        auto& prog = m_GLPrograms.unsharpMask;
        prog.Use();
        gl::BindProgramTextures(prog, {
            {&m_TexFBOs.lrSharpened.tex, uniforms::Image},
            {&m_TexFBOs.gaussianBlur.tex, uniforms::BlurredImage},
        });
        prog.SetUniform1f(uniforms::AmountMax, m_ProcessingSettings.unsharpMasking.amountMax);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    m_ProcessingOutputValid.unshMask = true;

    StartProcessing(ProcessingRequest::TONE_CURVE);
}

void c_OpenGLProcessing::StartToneMapping()
{
    m_ProcessingOutputValid.toneCurve = false;

    InitTextureAndFBO(m_TexFBOs.toneCurve, m_Selection.GetSize());
    m_TexFBOs.toneCurve.tex.SetLinearInterpolation(m_LinearInterpolationForDisplay);

    gl::c_FramebufferBinder binder(m_TexFBOs.toneCurve.fbo);

    auto& prog = m_GLPrograms.toneCurve;
    prog.Use();
    gl::BindProgramTextures(prog, { {&m_TexFBOs.unsharpMask.tex, uniforms::Image} });

    const auto& tcurve = m_ProcessingSettings.toneCurve;
    prog.SetUniform1i(uniforms::NumPoints, tcurve.GetNumPoints());
    prog.SetUniform1i(uniforms::Smooth, tcurve.GetSmooth());
    prog.SetUniform1i(uniforms::IsGamma, tcurve.IsGammaMode());
    prog.SetUniform1f(uniforms::Gamma, tcurve.GetGamma());
    glUniform2fv(prog.GetUniform(uniforms::CurvePoints), tcurve.GetNumPoints(), reinterpret_cast<const GLfloat*>(tcurve.GetPoints().data()));
    glUniform4fv(prog.GetUniform(uniforms::Splines), tcurve.GetNumPoints() - 1, reinterpret_cast<const GLfloat*>(tcurve.GetSplines().data()));

    m_VBOs.wholeSelectionAtZero.Bind();
    gl::SpecifyVertexAttribPointers();
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    m_ProcessingOutputValid.toneCurve = true;

    if (m_ProcessingCompletedHandler)
    {
        m_ProcessingCompletedHandler(CompletionStatus::COMPLETED);
    }

    if (m_ProgressTextHandler)
    {
        m_ProgressTextHandler(std::move(_("Idle")));
    }
}

void c_OpenGLProcessing::FillWholeSelectionVBO()
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

void c_OpenGLProcessing::SetSelection(wxRect selection)
{
    m_Selection = selection;
    FillWholeSelectionVBO();
}

void c_OpenGLProcessing::SetImage(const c_Image& img, bool linearInterpolation)
{
    m_Img = &img;

    /// Only buffers with no row padding are currently accepted.
    ///
    /// To change it, use appropriate glPixelStore* calls in `c_Texture` constructor
    /// before calling `glTexImage2D`.
    IMPPG_ASSERT(m_Img->GetBuffer().GetBytesPerRow() == m_Img->GetWidth() * sizeof(float));

    m_OriginalImg = gl::c_Texture::CreateMono(
        m_Img->GetWidth(),
        m_Img->GetHeight(),
        m_Img->GetBuffer().GetRow(0),
        linearInterpolation
    );
}

void c_OpenGLProcessing::SetTexturesLinearInterpolation(bool enable)
{
    m_LinearInterpolationForDisplay = enable;
    m_OriginalImg.SetLinearInterpolation(enable);
    m_TexFBOs.toneCurve.tex.SetLinearInterpolation(enable);
}

void c_OpenGLProcessing::SetProcessingSettings(ProcessingSettings settings)
{
    const bool recalculateUnshMaskGaussian = (m_ProcessingSettings.unsharpMasking.sigma != settings.unsharpMasking.sigma);
    const bool recalculateLRGaussian = (m_ProcessingSettings.LucyRichardson.sigma != settings.LucyRichardson.sigma);

    m_ProcessingSettings = settings;

    if (recalculateLRGaussian)
    {
        m_LRGaussian = CalculateHalf1DGaussianKernel(
            static_cast<int>(ceilf(m_ProcessingSettings.LucyRichardson.sigma) * 3),
            m_ProcessingSettings.LucyRichardson.sigma
        );
    }

    if (recalculateUnshMaskGaussian)
    {
        m_UnshMaskGaussian = CalculateHalf1DGaussianKernel(
            static_cast<int>(ceilf(m_ProcessingSettings.unsharpMasking.sigma) * 3),
            m_ProcessingSettings.unsharpMasking.sigma
        );
    }

    m_ProcessingSettings = settings;
}

c_Image c_OpenGLProcessing::GetProcessedSelection()
{
    IMPPG_ASSERT(m_Img != nullptr);

    m_LRSync.abortRequested = true;

    TexFbo temporary;
    const gl::c_Texture* srcTex{nullptr};

    if (!m_ProcessingOutputValid.toneCurve)
    {
        // It is unlikely, but possible, that we enter the function before any processing
        // has completed. In that case, just copy from the input image.

        InitTextureAndFBO(temporary, m_Selection.GetSize());
        m_VBOs.wholeSelection.Bind();
        gl::SpecifyVertexAttribPointers();
        auto& prog = m_GLPrograms.copy;
        prog.Use();
        gl::BindProgramTextures(prog, { {&m_OriginalImg, uniforms::Image} });
        gl::c_FramebufferBinder binder(temporary.fbo);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        srcTex = &temporary.tex;
    }
    else
    {
        srcTex = &m_TexFBOs.toneCurve.tex;
    }

    c_Image img(srcTex->GetWidth(), srcTex->GetHeight(), PixelFormat::PIX_MONO32F);
    glBindTexture(GL_TEXTURE_RECTANGLE, srcTex->Get());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_RECTANGLE, 0, GL_RED, GL_FLOAT, img.GetRow(0));

    return img;
}

} // namespace imppg::backend
