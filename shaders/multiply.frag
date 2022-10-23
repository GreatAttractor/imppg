/*
ImPPG (Image Post-Processor) - common operations for astronomical stacks and other images
Copyright (C) 2016-2022 Filip Szczerek <ga.software@yahoo.com>

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
    Fragment shader: multiply textures.
*/

#version 330 core

in vec2 TexCoord;
out vec4 Color;

uniform bool IsMono;
uniform sampler2DRect InputArray1;
uniform sampler2DRect InputArray2;

void main()
{
    if (IsMono)
    {
        float outputValue = texture(InputArray1, TexCoord).r * texture(InputArray2, TexCoord).r;
        Color = vec4(vec3(outputValue), 1.0);
    }
    else
    {
        vec3 outputValue = texture(InputArray1, TexCoord).rgb * texture(InputArray2, TexCoord).rgb;
        Color = vec4(outputValue, 1.0);
    }
}
