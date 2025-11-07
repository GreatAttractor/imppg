/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2025 Filip Szczerek <ga.software@yahoo.com>

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

#include "common/common.h"
#include "opengl/gl_common.h"
#include "opengl/opengl_proc.h"
#include "opengl/uniforms.h"
#include "math_utils/gauss.h"
#include "cpu_bmp/lrdeconv.h" //TODO: move BlurThresholdVicinity elsewhere
#include "common/imppg_assert.h"

#include <array>
#include <vector>

namespace imppg::backend {

std::unique_ptr<IProcessingBackEnd> CreateOpenGLProcessingBackend(unsigned lRCmdBatchSizeMpixIters)
{
    return std::make_unique<c_OpenGLProcessing>(lRCmdBatchSizeMpixIters);
}

/// Returns half of 1D Gaussian kernel (element [0] = peak value) with given sigma (std. deviation).
static std::vector<float> GetGaussianKernel(float sigma)
{
    return CalculateHalf1DGaussianKernel(static_cast<int>(ceilf(sigma) * 3), sigma);
}

void c_OpenGLProcessing::StartProcessing(c_Image img, ProcessingSettings procSettings)
{
    IMPPG_ASSERT(
        img.GetPixelFormat() == PixelFormat::PIX_MONO32F ||
        img.GetPixelFormat() == PixelFormat::PIX_RGB32F
    );
    SetSelection(img.GetImageRect(), true);
    SetImage(std::move(img), false);
    SetProcessingSettings(procSettings);
    StartProcessing(req_type::Sharpening{});
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
    if (!m_ProcessedOutput.has_value())
    {
        IMPPG_ASSERT(m_ProcessingOutputValid.toneCurve);

        const gl::c_Texture& srcTex = m_TexFBOs.toneCurve.tex;
        const auto pixFmt = m_Img->GetPixelFormat();
        m_ProcessedOutput = c_Image(srcTex.GetWidth(), srcTex.GetHeight(), pixFmt);
        glBindTexture(GL_TEXTURE_RECTANGLE, srcTex.Get());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glGetTexImage(GL_TEXTURE_RECTANGLE, 0, IsMono(pixFmt) ? GL_RED : GL_RGB, GL_FLOAT, m_ProcessedOutput.value().GetRow(0));
    }

    return m_ProcessedOutput.value();
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

c_OpenGLProcessing::c_OpenGLProcessing(unsigned lRCmdBatchSizeMpixIters)
: m_LRCmdBatchSizeMpixIters(lRCmdBatchSizeMpixIters)
{
    auto shaderDir = gl::GetShadersDirectory();

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
        {},
        "copy"
    );

    m_GLPrograms.toneCurve = gl::c_Program(
        { &m_GLShaders.frag.toneCurve,
          &m_GLShaders.vert.passthrough },
        { uniforms::IsMono,
          uniforms::Image,
          uniforms::NumPoints,
          uniforms::CurvePoints,
          uniforms::Splines,
          uniforms::Smooth,
          uniforms::IsGamma,
          uniforms::Gamma },
        {},
        "toneCurve"
    );

    m_GLPrograms.gaussianHorz = gl::c_Program(
        { &m_GLShaders.frag.gaussianHorz,
          &m_GLShaders.vert.passthrough },
        { uniforms::IsMono,
          uniforms::Image,
          uniforms::KernelRadius,
          uniforms::GaussianKernel },
        {},
        "gaussianHorz"
    );

    m_GLPrograms.gaussianVert = gl::c_Program(
        { &m_GLShaders.frag.gaussianVert,
          &m_GLShaders.vert.passthrough },
        { uniforms::Image,
          uniforms::KernelRadius,
          uniforms::GaussianKernel },
        {},
        "gaussianVert"
    );

    m_GLPrograms.unsharpMask = gl::c_Program(
        { &m_GLShaders.frag.unsharpMask,
          &m_GLShaders.vert.passthrough },
        { uniforms::IsMono,
          uniforms::Image,
          uniforms::BlurredImage,
          uniforms::InputImageBlurred,
          uniforms::SelectionPos,
          uniforms::Adaptive,
          uniforms::AmountMin,
          uniforms::AmountMax,
          uniforms::Threshold,
          uniforms::Width,
          uniforms::TransitionCurve
          },
        {},
        "unsharpMask"
    );

    m_GLPrograms.multiply = gl::c_Program(
        { &m_GLShaders.frag.multiply,
          &m_GLShaders.vert.passthrough },
        { uniforms::IsMono,
          uniforms::InputArray1,
          uniforms::InputArray2 },
        {},
        "multiply"
    );

    m_GLPrograms.divide = gl::c_Program(
        { &m_GLShaders.frag.divide,
          &m_GLShaders.vert.passthrough },
        { uniforms::IsMono,
          uniforms::InputArray1,
          uniforms::InputArray2 },
        {},
        "divide"
    );

    m_VBOs.wholeSelection = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);
    m_VBOs.wholeSelectionAtZero = gl::c_Buffer(GL_ARRAY_BUFFER, nullptr, 0, GL_DYNAMIC_DRAW);

    m_TexFBOs.unsharpMask.emplace_back(TexFbo{});

    m_LRGaussian = GetGaussianKernel(m_ProcessingSettings.LucyRichardson.sigma);
    m_UnshMaskData.emplace_back(UnsharpMaskData{
        GetGaussianKernel(m_ProcessingSettings.unsharpMask.at(0).sigma),
        GetAdaptiveUnshMaskTransitionCurve(m_ProcessingSettings.unsharpMask.at(0))
    });
}

void c_OpenGLProcessing::StartProcessing(ProcessingRequest procRequest)
{
    // if the previous processing step(s) did not complete, we have to execute it (them) first
    if (std::holds_alternative<req_type::ToneCurve>(procRequest) && !m_ProcessingOutputValid.unshMask)
    {
        procRequest = req_type::UnsharpMasking{0};
    }

    if (std::holds_alternative<req_type::UnsharpMasking>(procRequest) && !m_ProcessingOutputValid.sharpening)
    {
        procRequest = req_type::Sharpening{};
    }

    std::visit(Overload{
        [&](const req_type::Sharpening&) { StartLRDeconvolution(); },

        [&](const req_type::UnsharpMasking& umaskRequest) { StartUnsharpMasking(umaskRequest.maskIdx); },

        [&](const req_type::ToneCurve&) { StartToneMapping(); }
    }, procRequest);
}

bool c_OpenGLProcessing::InitTextureAndFBO(c_OpenGLProcessing::TexFbo& texFbo, const wxSize& size)
{
    if (!texFbo.tex || texFbo.tex.GetWidth() != size.GetWidth() || texFbo.tex.GetHeight() != size.GetHeight())
    {
        texFbo.tex = gl::c_Texture::Create(size.GetWidth(), size.GetHeight(), nullptr, false, IsMono(m_Img->GetPixelFormat()));
        texFbo.fbo = gl::c_Framebuffer({ &texFbo.tex });
        return true;
    }
    else
    {
        return false;
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

        const unsigned itersPerCmdBatch = std::max(1U, m_LRCmdBatchSizeMpixIters * 1'000'000 / (m_Selection.width * m_Selection.height));
        const unsigned itersInThisBatch = std::min(itersPerCmdBatch, m_LRSync.numIterationsLeft);

        for (unsigned i = 0; i < itersInThisBatch; i++)
        {
            GaussianConvolution(m_LRSync.prev->tex, m_TexFBOs.LR.estimateConvolved.fbo, m_LRGaussian);
            DivideTextures(m_TexFBOs.LR.original.tex, m_TexFBOs.LR.estimateConvolved.tex, m_TexFBOs.LR.convolvedDiv.fbo);
            GaussianConvolution(m_TexFBOs.LR.convolvedDiv.tex, m_TexFBOs.LR.convolved2.fbo, m_LRGaussian);
            MultiplyTextures(m_LRSync.prev->tex, m_TexFBOs.LR.convolved2.tex, m_LRSync.next->fbo);
            std::swap(m_LRSync.prev, m_LRSync.next);
        }

        // We need to force the execution of the GL commands issued so far, so that the user
        // can interrupt L-R processing via GUI after each command batch.
        // Otherwise, the GL driver could defer and clump all the above commands for (time-consuming)
        // execution together (testing on my system shows it happens with the SwapBuffers() call).
        glFinish();

        m_LRSync.numIterationsLeft -= itersInThisBatch;

        if (m_ProgressTextHandler)
        {
            const int iters = m_ProcessingSettings.LucyRichardson.iterations;
            const int percentage = 100 * (iters - m_LRSync.numIterationsLeft) / iters;
            m_ProgressTextHandler(wxString::Format(_(L"L\u2013R deconvolution: %d%%"), percentage));
        }
    }

    if (m_LRSync.numIterationsLeft == 0 && !m_ProcessingOutputValid.sharpening)
    {
        m_TexFBOs.lrSharpened.fbo.Bind();
        auto& prog = m_GLPrograms.copy;
        prog.Use();
        gl::BindProgramTextures(prog, { {&m_LRSync.next->tex, uniforms::Image} });
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        m_ProcessingOutputValid.sharpening = true;
        StartProcessing(req_type::UnsharpMasking{});
    }
}

void c_OpenGLProcessing::StartLRDeconvolution()
{
    if (m_ProgressTextHandler)
    {
        m_ProgressTextHandler(wxString::Format(_(L"L\u2013R deconvolution: %d%%"), 0));
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
        m_LRSync.abortRequested = true;
        m_VBOs.wholeSelection.Bind();
        gl::SpecifyVertexAttribPointers();
        auto& prog = m_GLPrograms.copy;
        prog.Use();
        gl::BindProgramTextures(prog, { {&m_OriginalImg, uniforms::Image} });
        m_TexFBOs.lrSharpened.fbo.Bind();
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        m_ProcessingOutputValid.sharpening = true;
        StartProcessing(req_type::UnsharpMasking{});
    }
    else
    {
        const gl::c_Texture& inputImg =
            (m_ProcessingSettings.LucyRichardson.deringing.enabled ?
            m_BlurredForDeringing.texture : m_OriginalImg);

        {
            if (m_ProcessingSettings.LucyRichardson.deringing.enabled)
            {
                m_VBOs.wholeSelectionAtZero.Bind();
            }
            else
            {
                m_VBOs.wholeSelection.Bind();
            }
            gl::SpecifyVertexAttribPointers();

            auto& prog = m_GLPrograms.copy;
            prog.Use();

            gl::BindProgramTextures(prog, { {&inputImg, uniforms::Image} });

            // Under "AMD PITCAIRN (DRM 2.50.0, 5.1.16-200.fc29.x86_64, LLVM 7.0.1)" renderer (Radeon R370, Fedora 29) cannot attach
            // the textures `LR.original` and `LR.buf1` to a single FBO and just render once - only the first attachment gets filled.
            // So we have to render to each separately.
            {
                m_TexFBOs.LR.original.fbo.Bind();
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
            {
                m_TexFBOs.LR.buf1.fbo.Bind();
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            }
        }

        // `buf1` and `buf2` are used as ping-pong buffers
        m_LRSync.prev = &m_TexFBOs.LR.buf1;
        m_LRSync.next = &m_TexFBOs.LR.buf2;
        m_LRSync.numIterationsLeft = m_ProcessingSettings.LucyRichardson.iterations;

        m_LRSync.abortRequested = false;
        IssueLRCommandBatch();
    }
}

void c_OpenGLProcessing::MultiplyTextures(const gl::c_Texture& tex1, const gl::c_Texture& tex2, gl::c_Framebuffer& dest)
{
    dest.Bind();
    auto& prog = m_GLPrograms.multiply;
    prog.Use();
    prog.SetUniform1i(uniforms::IsMono, IsMono(m_Img->GetPixelFormat()));
    gl::BindProgramTextures(prog, { {&tex1, uniforms::InputArray1}, {&tex2, uniforms::InputArray2} });
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void c_OpenGLProcessing::DivideTextures(const gl::c_Texture& tex1, const gl::c_Texture& tex2, gl::c_Framebuffer& dest)
{
    dest.Bind();
    auto& prog = m_GLPrograms.divide;
    prog.Use();
    prog.SetUniform1i(uniforms::IsMono, IsMono(m_Img->GetPixelFormat()));
    gl::BindProgramTextures(prog, { {&tex1, uniforms::InputArray1}, {&tex2, uniforms::InputArray2} });
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void c_OpenGLProcessing::GaussianConvolution(const gl::c_Texture& src, gl::c_Framebuffer& dest, const std::vector<float>& gaussian)
{
    InitTextureAndFBO(m_TexFBOs.aux, wxSize{src.GetWidth(), src.GetHeight()});

    // horizontal convolution: src -> aux
    {
        m_TexFBOs.aux.fbo.Bind();
        auto& prog = m_GLPrograms.gaussianHorz;
        prog.Use();
        gl::BindProgramTextures(prog, { {&src, uniforms::Image} });
        prog.SetUniform1i(uniforms::IsMono, IsMono(m_Img->GetPixelFormat()));
        prog.SetUniform1i(uniforms::KernelRadius, gaussian.size());
        glUniform1fv(prog.GetUniform(uniforms::GaussianKernel), gaussian.size(), gaussian.data());
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    // vertical convolution: aux -> dest
    {
        dest.Bind();
        auto& prog = m_GLPrograms.gaussianVert;
        prog.Use();
        gl::BindProgramTextures(prog, { {&m_TexFBOs.aux.tex, uniforms::Image} });
        prog.SetUniform1i(uniforms::KernelRadius, gaussian.size());
        glUniform1fv(prog.GetUniform(uniforms::GaussianKernel), gaussian.size(), gaussian.data());
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }
}


void c_OpenGLProcessing::StartUnsharpMasking(std::size_t maskIdx)
{
    m_ProcessingOutputValid.unshMask = false;
    m_ProcessingOutputValid.toneCurve = false;

    bool tfReinitialized = InitTextureAndFBO(m_TexFBOs.gaussianBlur, m_Selection.GetSize());
    for (TexFbo& umaskTexFbo: m_TexFBOs.unsharpMask)
    {
        tfReinitialized |= InitTextureAndFBO(umaskTexFbo, m_Selection.GetSize());
    }
    if (tfReinitialized) { maskIdx = 0; }

    m_VBOs.wholeSelectionAtZero.Bind();
    gl::SpecifyVertexAttribPointers();

    for (std::size_t i = maskIdx; i < m_ProcessingSettings.unsharpMask.size(); ++i)
    {
        const UnsharpMask& umask = m_ProcessingSettings.unsharpMask.at(i);

        const auto& inputTex = [&]() -> const gl::c_Texture& {
            if (0 == i) { return m_TexFBOs.lrSharpened.tex; }
            else { return m_TexFBOs.unsharpMask.at(i - 1).tex; }
        }();

        GaussianConvolution(inputTex, m_TexFBOs.gaussianBlur.fbo, m_UnshMaskData.at(i).gaussian);

        m_TexFBOs.unsharpMask.at(i).fbo.Bind();

        auto& prog = m_GLPrograms.unsharpMask;
        prog.Use();
        prog.SetUniform1i(uniforms::IsMono, IsMono(m_Img->GetPixelFormat()));

        gl::BindProgramTextures(prog, {
            {&inputTex, uniforms::Image},
            {&m_TexFBOs.gaussianBlur.tex, uniforms::BlurredImage},
            {&m_TexFBOs.inputBlurred.tex, uniforms::InputImageBlurred}
        });
        prog.SetUniform1i(uniforms::Adaptive, umask.adaptive);
        prog.SetUniform1f(uniforms::AmountMin, umask.amountMin);
        prog.SetUniform1f(uniforms::AmountMax, umask.amountMax);
        prog.SetUniform1f(uniforms::Threshold, umask.threshold);
        prog.SetUniform1f(uniforms::Width, umask.width);
        prog.SetUniform2i(uniforms::SelectionPos, m_Selection.GetLeft(), m_Selection.GetTop());
        const auto &trCurve = m_UnshMaskData.at(i).transitionCurve;
        prog.SetUniform4f(
            uniforms::TransitionCurve,
            trCurve[0],
            trCurve[1],
            trCurve[2],
            trCurve[3]
        );

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    m_ProcessingOutputValid.unshMask = true;

    StartProcessing(req_type::ToneCurve{});
}

void c_OpenGLProcessing::StartToneMapping()
{
    m_ProcessingOutputValid.toneCurve = false;

    InitTextureAndFBO(m_TexFBOs.toneCurve, m_Selection.GetSize());
    m_TexFBOs.toneCurve.tex.SetLinearInterpolation(m_LinearInterpolationForDisplay);

    m_TexFBOs.toneCurve.fbo.Bind();

    auto& prog = m_GLPrograms.toneCurve;
    prog.Use();
    gl::BindProgramTextures(prog, { {&checked_back(m_TexFBOs.unsharpMask).tex, uniforms::Image} });
    prog.SetUniform1i(uniforms::IsMono, IsMono(m_Img->GetPixelFormat()));

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
        m_ProgressTextHandler(_("Idle"));
    }
}

void c_OpenGLProcessing::FillSelectionVBOs()
{
    const GLfloat x0 = static_cast<GLfloat>(m_Selection.x);
    const GLfloat y0 = static_cast<GLfloat>(m_Selection.y);
    const GLfloat width = static_cast<GLfloat>(m_Selection.width);
    const GLfloat height = static_cast<GLfloat>(m_Selection.height);

    // For each buffer, 4 values per vertex: position (a full-screen quad), texture coords.

    const GLfloat vertexData[] = {
        -1.0f, -1.0f,    x0, y0,
        1.0f, -1.0f,     x0 + width, y0,
        1.0f, 1.0f,      x0 + width, y0 + height,
        -1.0f, 1.0f,     x0, y0 + height
    };
    m_VBOs.wholeSelection.SetData(vertexData, sizeof(vertexData), GL_DYNAMIC_DRAW);

    const GLfloat vertexDataAtZero[] = {
        -1.0f, -1.0f,    0, 0,
        1.0f, -1.0f,     width, 0,
        1.0f, 1.0f,      width, height,
        -1.0f, 1.0f,     0, height
    };
    m_VBOs.wholeSelectionAtZero.SetData(vertexDataAtZero, sizeof(vertexDataAtZero), GL_DYNAMIC_DRAW);
}

void c_OpenGLProcessing::SetSelection(wxRect selection, bool skipBlurForDeringing)
{
    m_Selection = selection;
    FillSelectionVBOs();

    m_BlurredForDeringing.workBuf.resize(m_Selection.width * m_Selection.height);

    if (m_Img)
    {
        m_BlurredForDeringing.image = c_Image(
            m_Selection.width,
            m_Selection.height,
            IsMono(m_Img->GetPixelFormat()) ? PixelFormat::PIX_MONO32F : PixelFormat::PIX_RGB32F
        );

        c_Image::Copy(
            *m_Img,
            m_BlurredForDeringing.image.value(),
            m_Selection.x,
            m_Selection.y,
            m_Selection.width,
            m_Selection.height,
            0,
            0
        );

        if (m_ProcessingSettings.LucyRichardson.deringing.enabled && !skipBlurForDeringing)
        {
            BlurForDeringing(m_ProcessingSettings.LucyRichardson.sigma);
        }
    }
}

void c_OpenGLProcessing::SetImage(c_Image img, bool linearInterpolation)
{
    IMPPG_ASSERT(
        img.GetPixelFormat() == PixelFormat::PIX_MONO32F ||
        img.GetPixelFormat() == PixelFormat::PIX_RGB32F
    );

    m_Img = std::move(img);

    m_ProcessedOutput = std::nullopt;

    const GLfloat imgWidth = static_cast<GLfloat>(m_Img->GetWidth());
    const GLfloat imgHeight = static_cast<GLfloat>(m_Img->GetHeight());
    // 4 values per vertex: position (a full-screen quad), texture coords
    const GLfloat vertexDataImg[] = {
        -1.0f, -1.0f,    0, 0,
        1.0f, -1.0f,     imgWidth, 0,
        1.0f, 1.0f,      imgWidth, imgHeight,
        -1.0f, 1.0f,     0, imgHeight
    };
    gl::c_Buffer wholeImageVBO = gl::c_Buffer(GL_ARRAY_BUFFER, vertexDataImg, sizeof(vertexDataImg), GL_DYNAMIC_DRAW);

    /// Only buffers with no row padding are currently accepted.
    ///
    /// To change it, use appropriate glPixelStore* calls in `c_Texture` constructor
    /// before calling `glTexImage2D`.
    IMPPG_ASSERT(m_Img->GetBuffer().GetBytesPerRow() ==
        m_Img->GetWidth() * NumChannels[static_cast<std::size_t>(m_Img->GetPixelFormat())] * sizeof(float));

    m_OriginalImg = gl::c_Texture::Create(
        m_Img->GetWidth(),
        m_Img->GetHeight(),
        m_Img->GetBuffer().GetRow(0),
        linearInterpolation,
        IsMono(m_Img->GetPixelFormat())
    );

    InitTextureAndFBO(m_TexFBOs.inputBlurred, m_Img->GetImageRect().GetSize());
    const auto gaussian = GetGaussianKernel(RAW_IMAGE_BLUR_SIGMA_FOR_ADAPTIVE_UNSHARP_MASK);
    wholeImageVBO.Bind();
    gl::SpecifyVertexAttribPointers();
    GaussianConvolution(m_OriginalImg, m_TexFBOs.inputBlurred.fbo, gaussian);

    auto& blurredImg = m_BlurredForDeringing.image;

    if (!blurredImg || blurredImg->GetPixelFormat() != m_Img->GetPixelFormat()
        || blurredImg->GetWidth() != static_cast<unsigned>(m_Selection.width)
        || blurredImg->GetHeight() != static_cast<unsigned>(m_Selection.height))
    {
        blurredImg = c_Image(
            m_Selection.width,
            m_Selection.height,
            IsMono(m_Img->GetPixelFormat()) ? PixelFormat::PIX_MONO32F : PixelFormat::PIX_RGB32F
        );
    }

    c_Image::Copy(
        *m_Img,
        m_BlurredForDeringing.image.value(),
        m_Selection.x,
        m_Selection.y,
        m_Selection.width,
        m_Selection.height,
        0,
        0
    );

    if (m_ProcessingSettings.LucyRichardson.deringing.enabled)
    {
        BlurForDeringing(m_ProcessingSettings.LucyRichardson.sigma);
    }
}

void c_OpenGLProcessing::SetTexturesLinearInterpolation(bool enable)
{
    m_LinearInterpolationForDisplay = enable;
    m_OriginalImg.SetLinearInterpolation(enable);
    m_TexFBOs.toneCurve.tex.SetLinearInterpolation(enable);
}

void c_OpenGLProcessing::SetProcessingSettings(ProcessingSettings settings)
{
    m_LRSync.abortRequested = true;

    const bool lrSigmaChanged = m_ProcessingSettings.LucyRichardson.sigma != settings.LucyRichardson.sigma;
    const bool deringingSwitchedOn = !m_ProcessingSettings.LucyRichardson.deringing.enabled
        && settings.LucyRichardson.deringing.enabled;

    if (lrSigmaChanged)
    {
        m_LRGaussian = GetGaussianKernel(settings.LucyRichardson.sigma);
    }

    if (lrSigmaChanged && settings.LucyRichardson.deringing.enabled || deringingSwitchedOn)
    {
        if (m_Img.has_value()) { BlurForDeringing(settings.LucyRichardson.sigma); }
    }

    if (settings.unsharpMask.size() != m_ProcessingSettings.unsharpMask.size())
    {
        m_TexFBOs.unsharpMask.clear();
        m_UnshMaskData.clear();
        for (std::size_t i = 0; i < settings.unsharpMask.size(); ++i)
        {
            m_TexFBOs.unsharpMask.emplace_back(TexFbo{});
            m_UnshMaskData.emplace_back(UnsharpMaskData{
                GetGaussianKernel(settings.unsharpMask.at(i).sigma),
                GetAdaptiveUnshMaskTransitionCurve(settings.unsharpMask.at(i))
            });
        }
    }
    else
    {
        for (std::size_t i = 0; i < settings.unsharpMask.size(); ++i)
        {
            if (m_ProcessingSettings.unsharpMask.at(i).sigma != settings.unsharpMask.at(i).sigma)
            {
                m_UnshMaskData.at(i).gaussian = GetGaussianKernel(settings.unsharpMask.at(i).sigma);
            }

            m_UnshMaskData.at(i).transitionCurve = GetAdaptiveUnshMaskTransitionCurve(settings.unsharpMask.at(i));
        }
    }

    m_ProcessingSettings = settings;
}

c_Image c_OpenGLProcessing::GetProcessedSelection()
{
    IMPPG_ASSERT(m_Img.has_value());

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
        temporary.fbo.Bind();
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

        srcTex = &temporary.tex;
    }
    else
    {
        srcTex = &m_TexFBOs.toneCurve.tex;
    }

    const auto pixFmt = m_Img->GetPixelFormat();
    c_Image img(srcTex->GetWidth(), srcTex->GetHeight(), pixFmt);
    glBindTexture(GL_TEXTURE_RECTANGLE, srcTex->Get());
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGetTexImage(GL_TEXTURE_RECTANGLE, 0, IsMono(pixFmt) ? GL_RED : GL_RGB, GL_FLOAT, img.GetRow(0));

    return img;
}

void c_OpenGLProcessing::BlurForDeringing(double sigma)
{
    IMPPG_ASSERT(m_Img.has_value());

    if (IsMono(m_Img->GetPixelFormat()))
    {
        BlurThresholdVicinity(
            c_View<const IImageBuffer>(m_Img->GetBuffer(), m_Selection),
            c_View(m_BlurredForDeringing.image.value().GetBuffer()),
            m_BlurredForDeringing.workBuf,
            DERINGING_BRIGHTNESS_THRESHOLD,
            sigma
        );
    }
    else
    {
        std::vector<c_Image> outChannel;
        for (int i = 0; i < 3; ++i)
        {
            outChannel.emplace_back(
                static_cast<unsigned>(m_Selection.width),
                static_cast<unsigned>(m_Selection.height),
                PixelFormat::PIX_MONO32F
            );
        };

        auto [inR, inG, inB] = m_Img->SplitRGB();
        std::array<c_Image, 3> inChannel{ std::move(inR), std::move(inG), std::move(inB) };

        #pragma omp parallel for
        for (int i = 0; i < 3; ++i)
        {
            BlurThresholdVicinity(
                c_View<const IImageBuffer>(inChannel.at(i).GetBuffer(), m_Selection),
                c_View(outChannel.at(i).GetBuffer()),
                m_BlurredForDeringing.workBuf,
                DERINGING_BRIGHTNESS_THRESHOLD,
                sigma
            );
        }

        m_BlurredForDeringing.image = c_Image::CombineRGB(outChannel.at(0), outChannel.at(1), outChannel.at(2));
    }

    m_BlurredForDeringing.texture = gl::c_Texture::Create(
        m_BlurredForDeringing.image.value().GetWidth(),
        m_BlurredForDeringing.image.value().GetHeight(),
        m_BlurredForDeringing.image.value().GetBuffer().GetRow(0),
        false,
        IsMono(m_Img->GetPixelFormat())
    );
}

} // namespace imppg::backend
