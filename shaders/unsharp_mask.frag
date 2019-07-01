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
    Fragment shader: perform usharp masking.
*/

#version 330 core

in vec2 TexCoord;
out vec4 Color;

uniform sampler2DRect Image;
uniform sampler2DRect BlurredImage;
uniform float AmountMin;
uniform float AmountMax;
uniform bool Adaptive;
uniform vec2 SelectionPos;

void main()
{
    float outputValue = AmountMax * texture(Image, TexCoord).r +
                        (1.0 - AmountMax) * texture(BlurredImage, TexCoord - SelectionPos).r;

    Color = vec4(outputValue, outputValue, outputValue, 1.0);
}
