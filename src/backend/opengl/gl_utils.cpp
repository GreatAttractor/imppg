/*
GPU-Assisted Ray Tracer
Copyright (C) 2016 Filip Szczerek <ga.software@yahoo.com>

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

#include <exception>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string.h>
#include <tuple>
#include <vector>

#include "../../imppg_assert.h"
#include "../../logging.h"
#include "gl_utils.h"

namespace imppg::backend::gl
{

static c_Buffer fullScreenQuadVertices;

/// Returns (file contents, file length).
static std::tuple<std::unique_ptr<GLchar[]>, GLint> ReadTextFile(const char* filename)
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

c_Shader::c_Shader(GLenum type, const char* srcFileName)
{
    auto [source, srcLength] = ReadTextFile(srcFileName);
    if (!source)
    {
        Log::Print(wxString::Format("Could not load shader from source file %s", srcFileName));
        throw std::runtime_error("Could not load shader");
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
            Log::Print(wxString::Format("Could not create shader from source file %s", srcFileName));
            std::cerr << "Shader compilation failed:\n\n" << infoLog.get() << std::endl;
            throw std::runtime_error("Could not create shader");
        }
    }
}

c_Program::c_Program(
    std::initializer_list<const c_Shader*> shaders,
    std::initializer_list<const char*> uniforms,
    std::initializer_list<const char*> attributes
)
{
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
        throw std::runtime_error("Failed to create GL shader program");
    }
    else
    {
        // Make sure the program contains all required uniforms and attributes.
        for (auto uniform: uniforms)
            IMPPG_ASSERT(-1 != (Uniforms[uniform] = glGetUniformLocation(m_Program.Get(), uniform)));
        for (auto attribute: attributes)
            IMPPG_ASSERT(-1 != (Attributes[attribute] = glGetAttribLocation(m_Program.Get(), attribute)));
    }
}

} // namespace imppg::backend::gl
