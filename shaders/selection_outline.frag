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
    Fragment shader: draw a dashed selection outline.
*/

#version 330 core

out vec4 Color;

const int DASH_LEN = 3;

void main()
{
    int xdiv = (int(gl_FragCoord.x) / DASH_LEN) % 2;
    int ydiv = (int(gl_FragCoord.y) / DASH_LEN) % 2;
    float outlineColor = (xdiv != ydiv) ? 1.0 : 0.0;

    Color = vec4(outlineColor, outlineColor, outlineColor, 1.0);
}
