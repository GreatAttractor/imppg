/*
GPU-Assisted Ray Tracer
Copyright (C) 2016-2025 Filip Szczerek <ga.software@yahoo.com>

This file is part of gpuart.

Gpuart is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Gpuart is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with gpuart.  If not, see <http://www.gnu.org/licenses/>.

File description:
    OpenGL utility classes implementation
*/

#include <boost/format.hpp>
#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string.h>
#include <tuple>
#include <vector>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "common/imppg_assert.h"
#include "logging/logging.h"
#include "opengl/gl_utils.h"

namespace imppg::backend::gl
{

std::unordered_map<GLenum, GLuint> c_Buffer::m_LastBoundBuffer;

/// Returns (file contents, file length).
static std::tuple<std::unique_ptr<GLchar[]>, GLint> ReadTextFile(const std::filesystem::path& filename)
{
    std::ifstream file(filename, std::ios_base::in | std::ios_base::binary);
    if (file.fail())
        return { nullptr, 0 };

    file.seekg(0, std::ios_base::end);
    auto length = static_cast<GLint>(file.tellg());

    file.seekg(0, std::ios_base::beg);
    auto contents = std::make_unique<GLchar[]>(length);
    file.read(contents.get(), length);

    return { std::move(contents), std::move(length) };
}

c_Shader::c_Shader(GLenum type, const std::filesystem::path& srcFileName)
{
    auto [source, srcLength] = ReadTextFile(srcFileName);
    if (!source)
    {
        const wxString msg = wxString::Format("Could not load shader: %s", srcFileName.generic_string());
        Log::Print(msg);
        throw std::runtime_error(msg);
    }
    else
    {
        m_Shader.Get() = glCreateShader(type);
        GLchar* srcPtr = source.get();
        glShaderSource(m_Shader.Get(), 1, &srcPtr, &srcLength);
        glCompileShader(m_Shader.Get());
        GLint success = GL_FALSE;
        glGetShaderiv(m_Shader.Get(), GL_COMPILE_STATUS, &success);
        if (!success)
        {
            GLint logLength;
            glGetShaderiv(m_Shader.Get(), GL_INFO_LOG_LENGTH, &logLength);
            auto infoLog = std::make_unique<char[]>(logLength);
            glGetShaderInfoLog(m_Shader.Get(), logLength, nullptr, infoLog.get());
            Log::Print(wxString::Format("Could not create shader from source file %s", srcFileName.generic_string()));
            std::cerr << "Shader compilation failed:\n\n" << infoLog.get() << std::endl;
            throw std::runtime_error("Could not create shader.");
        }
    }
}

c_Program::c_Program(
    std::initializer_list<const c_Shader*> shaders,
    std::initializer_list<const char*> uniforms,
    std::initializer_list<const char*> attributes,
    std::string name
)
{
    m_Name = name;

    m_Program.Get() = glCreateProgram();
    for (const auto* shader: shaders)
        glAttachShader(m_Program.Get(), shader->Get());

    glLinkProgram(m_Program.Get());

    GLint success = GL_FALSE;
    glGetProgramiv(m_Program.Get(), GL_LINK_STATUS, &success);

    if (!success)
    {
        GLint logLength;
        glGetProgramiv(m_Program.Get(), GL_INFO_LOG_LENGTH, &logLength);
        auto infoLog = std::make_unique<GLchar[]>(logLength);
        glGetProgramInfoLog(m_Program.Get(), logLength, nullptr, infoLog.get());
        std::cerr << "Program linking failed:\n\n" << infoLog.get() << std::endl;
        throw std::runtime_error("Failed to create GL shader program.");
    }
    else
    {
        // Make sure the program contains all required uniforms and attributes.
        for (auto uniform: uniforms)
        {
            IMPPG_ASSERT_MSG(
                -1 != (Uniforms[uniform] = glGetUniformLocation(m_Program.Get(), uniform)),
                boost::str(boost::format("Uniform \"%s\" not found in compiled program \"%s\" - perhaps it is not used?")
                    % uniform % m_Name.c_str())
            );
        }
        for (auto attribute: attributes)
        {
            IMPPG_ASSERT_MSG(
                -1 != (Attributes[attribute] = glGetAttribLocation(m_Program.Get(), attribute)),
                boost::str(boost::format("Attribute \"%s\" not found in compiled program \"%s\" - perhaps it is not used?")
                    % attribute % m_Name.c_str())
            );
        }
    }
}

c_Framebuffer::c_Framebuffer(std::initializer_list<c_Texture*> attachedTextures)
{
    GLint maxColorAttachments;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
    IMPPG_ASSERT(static_cast<int>(attachedTextures.size()) <= maxColorAttachments);
    IMPPG_ASSERT(attachedTextures.size() > 0);

    m_Width = (*attachedTextures.begin())->GetWidth();
    m_Height = (*attachedTextures.begin())->GetHeight();

    glGenFramebuffers(1, &m_Framebuffer.Get());
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_Framebuffer.Get());

    std::vector<GLenum> attachments(attachedTextures.size());

    for (size_t i = 0; i < attachedTextures.size(); i++)
    {
        glFramebufferTexture(
            GL_DRAW_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0 + i,
            static_cast<GLuint>((*(attachedTextures.begin() + i))->Get()),
            0
        );
        attachments[i] = GL_COLOR_ATTACHMENT0 + i;
    }
    glDrawBuffers(attachments.size(), attachments.data());

    GLenum fbStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
    if (fbStatus != GL_FRAMEBUFFER_COMPLETE)
    {
        Log::Print("Could not create FBO, status: %d\n", static_cast<int>(fbStatus));
        throw std::runtime_error("Could not create FBO.");
    }

    m_NumAttachedTextures = attachedTextures.size();
}

void c_Framebuffer::Bind()
{
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_Framebuffer.Get());
    glViewport(0, 0, m_Width, m_Height);
}

void c_Texture::SetLinearInterpolation(bool enabled)
{
    glBindTexture(GL_TEXTURE_RECTANGLE, m_Texture.Get());
    const GLint interpolation = enabled ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, interpolation);
    glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, interpolation);
}

void BindProgramTextures(c_Program& program, std::initializer_list<std::pair<const c_Texture*, const char*>> texUniforms)
{
    int textureUnit = 0;

    for (const auto& texUniform: texUniforms)
    {
        glActiveTexture(GL_TEXTURE0 + textureUnit);
        glBindTexture(GL_TEXTURE_RECTANGLE, texUniform.first->Get());
        program.SetUniform1i(texUniform.second, textureUnit);
        ++textureUnit;
    }
}

wxFileName GetShadersDirectory()
{
    auto shaderDir = wxFileName(wxStandardPaths::Get().GetExecutablePath());
    shaderDir.AppendDir("shaders");
    if (!shaderDir.Exists())
    {
        shaderDir.AssignCwd();
        shaderDir.AppendDir("shaders");
        if (!shaderDir.Exists())
        {
            shaderDir.AssignDir(IMPPG_SHADERS_DIR); // defined in CMakeLists.txt
        }
    }

    return shaderDir;
}

} // namespace imppg::backend::gl
