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
    Vertex shader: show scaled image.
*/

#version 330 core

/// Logical image coordinates including scaling: (0, 0) to (img_w * zoom, img_h * zoom).
layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 in_TexCoord;

out vec2 TexCoord;

/// Scrolled position of the image view area.
uniform ivec2 ScrollPos;
uniform ivec2 ViewportSize;

void main()
{
    gl_Position = vec4(
        2.0 / ViewportSize.x * Position.x - 1.0 - 2.0 * ScrollPos.x / ViewportSize.x,
        -2.0 / ViewportSize.y * Position.y + 1.0 + 2.0 * ScrollPos.y / ViewportSize.y,
        0.0,
        1.0
    );

    TexCoord.xy = in_TexCoord.xy;
}
