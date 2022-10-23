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
    Fragment shader: show mono image with bicubic interpolation.
*/

#version 330 core

in vec2 TexCoord;
out vec4 Color;

uniform bool IsMono;
uniform sampler2DRect Image;

/// Returns interpolated value at 0<=t<=1, where f(-1) = fm1, f(0) = f0, f(1) = f1, f(2) = f2.
float CubicInterpScalar(float t, float fm1, float f0, float f1, float f2)
{
    float delta_k = f1 - f0;
    float dk = (f1 - fm1) / 2;
    float dk1 = (f2 - f0) / 2;
    float a0 = f0;
    float a1 = dk;
    float a2 = 3 * delta_k - 2 * dk - dk1;
    float a3 = dk + dk1 - 2 * delta_k;

    return t * (t * (a3 * t + a2) + a1) + a0;
}

/// Returns interpolated value at 0<=t<=1, where f(-1) = fm1, f(0) = f0, f(1) = f1, f(2) = f2.
vec3 CubicInterpVec(float t, vec3 fm1, vec3 f0, vec3 f1, vec3 f2)
{
    vec3 delta_k = f1 - f0;
    vec3 dk = (f1 - fm1) / 2;
    vec3 dk1 = (f2 - f0) / 2;
    vec3 a0 = f0;
    vec3 a1 = dk;
    vec3 a2 = 3 * delta_k - 2 * dk - dk1;
    vec3 a3 = dk + dk1 - 2 * delta_k;

    return t * (t * (a3 * t + a2) + a1) + a0;
}

void main()
{
    vec2 tc0 = floor(TexCoord - vec2(0.5, 0.5));

    vec2 cmm = tc0 + vec2(-1, -1);
    vec2 cm0 = tc0 + vec2(-1,  0);
    vec2 cm1 = tc0 + vec2(-1,  1);
    vec2 cm2 = tc0 + vec2(-1,  2);
    vec2 c0m = tc0 + vec2( 0, -1);
    vec2 c00 = tc0 + vec2( 0,  0);
    vec2 c01 = tc0 + vec2( 0,  1);
    vec2 c02 = tc0 + vec2( 0,  2);
    vec2 c1m = tc0 + vec2( 1, -1);
    vec2 c10 = tc0 + vec2( 1,  0);
    vec2 c11 = tc0 + vec2( 1,  1);
    vec2 c12 = tc0 + vec2( 1,  2);
    vec2 c2m = tc0 + vec2( 2, -1);
    vec2 c20 = tc0 + vec2( 2,  0);
    vec2 c21 = tc0 + vec2( 2,  1);
    vec2 c22 = tc0 + vec2( 2,  2);

    float tx = TexCoord.x - 0.5 - tc0.x;
    float ty = TexCoord.y - 0.5 - tc0.y;

    if (IsMono)
    {
        float interpHorz_m = CubicInterpScalar(tx, texture(Image, cmm).r, texture(Image, c0m).r, texture(Image, c1m).r, texture(Image, c2m).r);
        float interpHorz_0 = CubicInterpScalar(tx, texture(Image, cm0).r, texture(Image, c00).r, texture(Image, c10).r, texture(Image, c20).r);
        float interpHorz_1 = CubicInterpScalar(tx, texture(Image, cm1).r, texture(Image, c01).r, texture(Image, c11).r, texture(Image, c21).r);
        float interpHorz_2 = CubicInterpScalar(tx, texture(Image, cm2).r, texture(Image, c02).r, texture(Image, c12).r, texture(Image, c22).r);

        float value = CubicInterpScalar(ty, interpHorz_m, interpHorz_0, interpHorz_1, interpHorz_2);
        Color = vec4(vec3(value), 1.0);
    }
    else
    {
        vec3 interpHorz_m = CubicInterpVec(tx, texture(Image, cmm).rgb, texture(Image, c0m).rgb, texture(Image, c1m).rgb, texture(Image, c2m).rgb);
        vec3 interpHorz_0 = CubicInterpVec(tx, texture(Image, cm0).rgb, texture(Image, c00).rgb, texture(Image, c10).rgb, texture(Image, c20).rgb);
        vec3 interpHorz_1 = CubicInterpVec(tx, texture(Image, cm1).rgb, texture(Image, c01).rgb, texture(Image, c11).rgb, texture(Image, c21).rgb);
        vec3 interpHorz_2 = CubicInterpVec(tx, texture(Image, cm2).rgb, texture(Image, c02).rgb, texture(Image, c12).rgb, texture(Image, c22).rgb);

        vec3 value = CubicInterpVec(ty, interpHorz_m, interpHorz_0, interpHorz_1, interpHorz_2);
        Color = vec4(value, 1.0);
    }
}
