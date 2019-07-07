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
    Vertex shader: pass-through.
*/

#version 330 core

layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 in_TexCoord;

out vec2 TexCoord;

void main()
{
    gl_Position.xy = Position.xy;
    gl_Position.z = 0.0;
    gl_Position.w = 1.0;
    TexCoord.xy = in_TexCoord;
}
