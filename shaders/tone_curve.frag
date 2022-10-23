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
    Fragment shader: apply tone curve.
*/

#version 330 core

in vec2 TexCoord;
out vec4 Color;

const int MAX_CURVE_POINTS = 64;

uniform sampler2DRect Image;

uniform int NumPoints;
uniform bool Smooth;
uniform vec2 CurvePoints[MAX_CURVE_POINTS]; // only `NumPoints` elements are valid
uniform vec4 Splines[MAX_CURVE_POINTS]; // only `NumPoints-1` elements are valid
uniform bool IsGamma;
uniform float Gamma;

float map_component(float inputValue)
{
    float outputValue = 0;

    if (IsGamma)
    {
        vec2 p0 = CurvePoints[0];
        vec2 p1 = CurvePoints[1];

        if (inputValue < p0.x)
            outputValue = p0.y;
        else if (inputValue >= p1.x)
            outputValue = p1.y;
        else
            outputValue = p0.y + pow((inputValue - p0.x) / (p1.x - p0.x), 1 / Gamma) * (p1.y - p0.y);
    }
    else
    {
        int nextIdx = NumPoints; // index of the first curve point whose X >= inputValue
        for (int i = 0; i < NumPoints; i++)
        {
            if (CurvePoints[i].x >= inputValue)
            {
                nextIdx = i;
                break;
            }
        }

        // if `input` is at or past the last point, return the value at the last point
        if (nextIdx == NumPoints)
            outputValue = CurvePoints[NumPoints - 1].y;
        else if (nextIdx == 0)
            // if `input` is at or before the first point, return the value at the first point
            outputValue = CurvePoints[0].y;
        else
        {
            float deltaX = CurvePoints[nextIdx].x - CurvePoints[nextIdx - 1].x;

            if (!Smooth) // the curve is piecewise linear
            {
                float deltaY = CurvePoints[nextIdx].y - CurvePoints[nextIdx - 1].y;
                outputValue = CurvePoints[nextIdx - 1].y + deltaY * (inputValue - CurvePoints[nextIdx - 1].x) / deltaX;
            }
            else // the curve consists of Catmull-Rom splines
            {
                float t = (inputValue - CurvePoints[nextIdx - 1].x) / deltaX;

                vec4 spline = Splines[nextIdx - 1];
                outputValue = t * (t * (t * spline.r + spline.g) + spline.b) + spline.a;
            }
        }
    }

    return clamp(outputValue, 0.0, 1.0);
}

void main()
{
    vec3 inValue = texture(Image, TexCoord).rgb;
    Color = vec4(
        map_component(inValue.r),
        map_component(inValue.g),
        map_component(inValue.b),
        1.0
    );
}
