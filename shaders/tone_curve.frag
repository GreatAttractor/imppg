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

void main()
{
    float inputValue = texture(Image, TexCoord).r;
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
            if (CurvePoints[i].x >= inputValue)
            {
                nextIdx = i;
                break;
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

    Color = vec4(outputValue, outputValue, outputValue, 1.0);
}
