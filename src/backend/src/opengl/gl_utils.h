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
    OpenGL utility classes header.
*/

#ifndef IMPPG_GL_UTILS_HEADER
#define IMPPG_GL_UTILS_HEADER

#include <GL/glew.h>
#include <initializer_list>
#include <memory>
#include <unordered_map>
#include <utility>
#include <wx/filename.h>

#include "common/imppg_assert.h"

namespace imppg::backend::gl {

wxFileName GetShadersDirectory();

/// Wrapper of an OpenGL object; movable, non-copyable.
template<void Deleter(GLuint)>
class c_Wrapper
{
    GLuint m_Handle{0};

public:

    explicit operator bool() const { return m_Handle > 0; }

    c_Wrapper(): m_Handle(0) { }
    c_Wrapper(GLuint obj): m_Handle(obj) { }

    c_Wrapper(const c_Wrapper&)            = delete;
    c_Wrapper& operator=(const c_Wrapper&) = delete;

    c_Wrapper(c_Wrapper&& w)
    {
        Deleter(m_Handle);
        m_Handle = w.m_Handle;
        w.m_Handle = 0;
    }

    c_Wrapper& operator=(c_Wrapper&& w)
    {
        Deleter(m_Handle);
        m_Handle = w.m_Handle;
        w.m_Handle = 0;
        return *this;
    }

    ~c_Wrapper() { Deleter(m_Handle); }

    GLuint& Get() { return m_Handle; }

    GLuint GetConst() const { return m_Handle; }

    void Delete() { Deleter(m_Handle); m_Handle = 0; }
};

class c_Buffer
{
    static void Deleter(GLuint obj) { if (obj != 0) glDeleteBuffers(1, &obj); }
    c_Wrapper<Deleter> m_Buffer;
    GLenum m_Target;

    static std::unordered_map<GLenum, GLuint> m_LastBoundBuffer;

public:

    explicit operator bool() const { return static_cast<bool>(m_Buffer); }

    c_Buffer() = default;

    c_Buffer(const c_Buffer&)            = delete;
    c_Buffer& operator=(const c_Buffer&) = delete;
    c_Buffer(c_Buffer&&)                 = default;
    c_Buffer& operator=(c_Buffer&&)      = default;

    c_Buffer(GLenum target, const GLvoid* data, GLsizei size, GLenum usage)
    {
        m_Target = target;
        glGenBuffers(1, &m_Buffer.Get());
        glBindBuffer(target, m_Buffer.Get());
        m_LastBoundBuffer[m_Target] = m_Buffer.Get();
        glBufferData(target, size, data, usage);
    }

    void SetData(const GLvoid* data, GLsizei size, GLenum usage)
    {
        IMPPG_ASSERT(m_Buffer);
        Bind();
        glBufferData(m_Target, size, data, usage);
    }

    void Bind()
    {
        glBindBuffer(m_Target, m_Buffer.Get());
        m_LastBoundBuffer[m_Target] = m_Buffer.Get();
    }

    bool IsBound() const
    {
        const auto loc = m_LastBoundBuffer.find(m_Target);
        if (loc != m_LastBoundBuffer.end())
        {
            return loc->second == m_Buffer.GetConst();
        }
        else
        {
            return false;
        }
    }

    GLuint Get() const { return m_Buffer.GetConst(); }
};

class c_Texture
{
    static void Deleter(GLuint obj) { if (obj != 0) glDeleteTextures(1, &obj); }
    c_Wrapper<Deleter> m_Texture;
    int m_Width{0};
    int m_Height{0};

public:

    explicit operator bool() const { return static_cast<bool>(m_Texture); }

    c_Texture() = default;

    c_Texture(const c_Texture&)            = delete;
    c_Texture& operator=(const c_Texture&) = delete;
    c_Texture(c_Texture&&)                 = default;
    c_Texture& operator=(c_Texture&&)      = default;

    /// Creates a rectangle texture.
    static c_Texture Create(GLsizei width, GLsizei height, const GLvoid* data, bool linearInterpolation, bool mono)
    {
        // using RGBA instead of just RGB, because Intel HD Graphics 5500 (Broadwell GT2) + Mesa 11.1.0 (git-525f3c2)
        // cannot create a framebuffer with RGB32F color attachments
        GLenum format = mono ? GL_RED : GL_RGB;
        GLenum internalFormat = mono ? GL_R32F : GL_RGBA32F;

        return c_Texture(internalFormat, width, height, format, GL_FLOAT, data, linearInterpolation);
    }

    /// Creates a rectangle texture.
    c_Texture(GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid* data, bool linearInterpolation = false)
    {
        glGenTextures(1, &m_Texture.Get());
        IMPPG_ASSERT(m_Texture);
        glBindTexture(GL_TEXTURE_RECTANGLE, m_Texture.Get());
        glTexImage2D(GL_TEXTURE_RECTANGLE, 0, internalFormat, width, height, 0, format, type, data);

        const GLint interpolation = linearInterpolation ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MIN_FILTER, interpolation);
        glTexParameteri(GL_TEXTURE_RECTANGLE, GL_TEXTURE_MAG_FILTER, interpolation);

        m_Width = width;
        m_Height = height;
    }

    /// Creates a buffer texture.
    c_Texture(GLenum internalFormat, const c_Buffer&buffer)
    {
        glGenTextures(1, &m_Texture.Get());
        glBindTexture(GL_TEXTURE_BUFFER, m_Texture.Get());
        glTexBuffer(GL_TEXTURE_BUFFER, internalFormat, buffer.Get());
    }

    void SetLinearInterpolation(bool enabled);

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

    GLuint Get() const { return m_Texture.GetConst(); }
};

class c_Shader
{
    static void Deleter(GLuint obj) { if (obj != 0) glDeleteShader(obj); }
    c_Wrapper<Deleter> m_Shader;

public:

    explicit operator bool() const { return static_cast<bool>(m_Shader); }

    c_Shader() = default;

    c_Shader(const c_Shader&)            = delete;
    c_Shader& operator=(const c_Shader&) = delete;
    c_Shader(c_Shader&&)                 = default;
    c_Shader& operator=(c_Shader&&)      = default;

    c_Shader(GLenum type, const std::filesystem::path& srcFileName);

    GLuint Get() const { return m_Shader.GetConst(); }
};

class VertexArrayObj
{
    static void Deleter(GLuint obj) { if (obj != 0) glDeleteVertexArrays(1, &obj); }
    c_Wrapper<Deleter> m_VAO;

public:
    explicit operator bool() const { return static_cast<bool>(m_VAO); }

    VertexArrayObj() = default;
    void Init()
    {
        m_VAO.Delete();
        glGenVertexArrays(1, &m_VAO.Get());
    }

    VertexArrayObj(const VertexArrayObj&)            = delete;
    VertexArrayObj& operator=(const VertexArrayObj&) = delete;
    VertexArrayObj(VertexArrayObj&&)                 = default;
    VertexArrayObj& operator=(VertexArrayObj&&)      = default;

    void Bind() { glBindVertexArray(m_VAO.Get()); }
};

class c_Program
{
    static void Deleter(GLuint obj) { if (obj != 0) glDeleteProgram(obj); }
    c_Wrapper<Deleter> m_Program;

    std::unordered_map<const char*, GLint> Uniforms;
    std::unordered_map<const char*, GLint> Attributes;

    std::string m_Name;

public:
    explicit operator bool() const { return static_cast<bool>(m_Program); }

    c_Program() = default;

    c_Program(const c_Program&)            = delete;
    c_Program& operator=(const c_Program&) = delete;
    c_Program(c_Program&&)                 = default;
    c_Program& operator=(c_Program&&)      = default;

    c_Program(std::initializer_list<const c_Shader*> shaders,
              std::initializer_list<const char*> uniforms,
              std::initializer_list<const char*> attributes,
              std::string name);

    void SetUniform1i(const char* uniform, GLint value)
    {
        IMPPG_ASSERT(Uniforms.find(uniform) != Uniforms.end());
        glUniform1i(Uniforms[uniform], value);
    }

    void SetUniform1ui(const char* uniform, GLuint value)
    {
        IMPPG_ASSERT(Uniforms.find(uniform) != Uniforms.end());
        glUniform1ui(Uniforms[uniform], value);
    }

    void SetUniform1f(const char* uniform, GLfloat f)
    {
        IMPPG_ASSERT(Uniforms.find(uniform) != Uniforms.end());
        glUniform1f(Uniforms[uniform], f);
    }

    void SetUniform2f(const char* uniform, GLfloat f0, GLfloat f1)
    {
        IMPPG_ASSERT(Uniforms.find(uniform) != Uniforms.end());
        glUniform2f(Uniforms[uniform], f0, f1);
    }

    void SetUniform2i(const char* uniform, GLint i0, GLint i1)
    {
        IMPPG_ASSERT(Uniforms.find(uniform) != Uniforms.end());
        glUniform2i(Uniforms[uniform], i0, i1);
    }

    void SetUniform3f(const char* uniform, GLfloat f0, GLfloat f1, GLfloat f2)
    {
        IMPPG_ASSERT(Uniforms.find(uniform) != Uniforms.end());
        glUniform3f(Uniforms[uniform], f0, f1, f2);
    }

    void SetUniform4f(const char* uniform, GLfloat f0, GLfloat f1, GLfloat f2, GLfloat f3)
    {
        IMPPG_ASSERT(Uniforms.find(uniform) != Uniforms.end());
        glUniform4f(Uniforms[uniform], f0, f1, f2, f3);
    }

    GLint GetUniform(const char* uniform) const { return Uniforms.find(uniform)->second; }

    GLint GetAttribute(const char* attribute) const { return Attributes.find(attribute)->second; }

    void Use()
    {
        glUseProgram(m_Program.Get());
    }

    void Unbind() { glUseProgram(0); }

    const std::string& GetName() const { return m_Name; }
};

class c_Framebuffer
{
    static void Deleter(GLuint obj) { glDeleteFramebuffers(1, &obj); }
    c_Wrapper<Deleter> m_Framebuffer;

    size_t m_NumAttachedTextures;

    int m_Width{0};
    int m_Height{0};

public:
    explicit operator bool() const { return static_cast<bool>(m_Framebuffer); }

    c_Framebuffer() = default;

    c_Framebuffer(const c_Framebuffer&)             = delete;
    c_Framebuffer& operator=(const c_Framebuffer &) = delete;
    c_Framebuffer(c_Framebuffer&&)                  = default;
    c_Framebuffer& operator=(c_Framebuffer&&)       = default;

    c_Framebuffer(std::initializer_list<c_Texture*> attachedTextures);

    void Bind();

    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }

    GLuint Get() const { return m_Framebuffer.GetConst(); }
};

/// For the given `program`, binds textures and assigns them to corresponding uniforms.
void BindProgramTextures(c_Program& program, std::initializer_list<std::pair<const c_Texture*, const char*>> texUniforms);

} // namespace imppg::backend::gl

#endif // IMPPG_GL_UTILS_HEADER
