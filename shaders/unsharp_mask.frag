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
    Fragment shader: perform unsharp masking.
*/

#version 330 core

in vec2 TexCoord;
out vec4 Color;

uniform sampler2DRect Image;
uniform sampler2DRect BlurredImage;

// whole input image, blurred; the fragment corresponding to `Image` and
// `BlurredImage` is at `SelectionPos`
uniform sampler2DRect InputImageBlurred;

uniform ivec2 SelectionPos;
uniform float AmountMin;
uniform float AmountMax;
uniform bool Adaptive;
uniform float Threshold;
uniform float Width;

// coefficients of a cubic curve; see `GetAdaptiveUnshMaskTransitionCurve`
// in `common.h` for details
uniform vec4 TransitionCurve;

void main()
{
    float amount = 1.0;

    if (!Adaptive)
    {
        amount = AmountMax;
    }
    else
    {
        float l = texture(InputImageBlurred, TexCoord + vec2(SelectionPos)).r;
        float a = TransitionCurve.x;
        float b = TransitionCurve.y;
        float c = TransitionCurve.z;
        float d = TransitionCurve.w;

        if (l < Threshold - Width)
            amount = AmountMin;
        else if (l > Threshold + Width)
            amount = AmountMax;
        else
            amount = l * (l * (a * l + b) + c) + d;
    }

    float outputValue = amount * texture(Image, TexCoord).r +
        (1.0 - amount) * texture(BlurredImage, TexCoord).r;

    outputValue = clamp(outputValue, 0.0, 1.0);
    Color = vec4(outputValue, outputValue, outputValue, 1.0);
}
