#version 330 core

in vec2 TexCoord;
out vec4 Color;

uniform sampler2DRect Image;

/// Returns interpolated value at 0<=t<=1, where f(-1) = fm1, f(0) = f0, f(1) = f1, f(2) = f2.
float CubicInterp(float t, float fm1, float f0, float f1, float f2)
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

    float interpHorz_m = CubicInterp(tx, texture(Image, cmm).r, texture(Image, c0m).r, texture(Image, c1m).r, texture(Image, c2m).r);
    float interpHorz_0 = CubicInterp(tx, texture(Image, cm0).r, texture(Image, c00).r, texture(Image, c10).r, texture(Image, c20).r);
    float interpHorz_1 = CubicInterp(tx, texture(Image, cm1).r, texture(Image, c01).r, texture(Image, c11).r, texture(Image, c21).r);
    float interpHorz_2 = CubicInterp(tx, texture(Image, cm2).r, texture(Image, c02).r, texture(Image, c12).r, texture(Image, c22).r);

    float value = CubicInterp(ty, interpHorz_m, interpHorz_0, interpHorz_1, interpHorz_2);
    Color = vec4(value, value, value, 1.0);
}
