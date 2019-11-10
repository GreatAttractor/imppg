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
    OpenGL back end common functions.
*/

#ifndef IMPPG_GL_COMMON_H
#define IMPPG_GL_COMMON_H

#include <GL/glew.h>

namespace imppg::backend::gl {

namespace attributes
{
    constexpr GLuint Position = 0;
    constexpr GLuint TexCoord = 1;
}

inline void InitVAO()
{
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glEnableVertexAttribArray(attributes::Position);
    glEnableVertexAttribArray(attributes::TexCoord);
}

inline void SpecifyVertexAttribPointers()
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

} // namespace imppg::backend::gl

#endif  // IMPPG_GL_COMMON_H
