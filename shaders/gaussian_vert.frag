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
    Fragment shader: vertical Gaussian convolution (blur).
*/

#version 330 core

in vec2 TexCoord;
out vec4 Color;

const int MAX_GAUSSIAN_RADIUS = 30;

uniform bool IsMono;
uniform sampler2DRect Image;
uniform int KernelRadius;
uniform float GaussianKernel[MAX_GAUSSIAN_RADIUS]; // only `KernelRadius` elements are valid; element [0] = max value

void main()
{
    if (IsMono)
    {
        vec3 outputValue = vec3(0.0);

        for (int i = -(KernelRadius - 1); i < KernelRadius; i++)
        {
            outputValue += GaussianKernel[abs(i)] * texture(Image, TexCoord + vec2(0, float(i))).rgb;
        }

        Color = vec4(outputValue, 1.0);
    }
    else
    {
        vec3 outputValue = vec3(0.0);

        for (int i = -(KernelRadius - 1); i < KernelRadius; i++)
        {
            outputValue += GaussianKernel[abs(i)] * texture(Image, TexCoord + vec2(0, float(i))).rgb;
        }

        Color = vec4(outputValue, 1.0);
    }
}
