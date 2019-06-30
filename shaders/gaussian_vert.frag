#version 330 core

in vec2 TexCoord;
out vec4 Color;

const int MAX_GAUSSIAN_RADIUS = 30;

uniform sampler2DRect Image;
uniform int KernelRadius;
uniform float GaussianKernel[MAX_GAUSSIAN_RADIUS]; // only `KernelRadius` elements are valid; element [0] = max value

void main()
{
    float outputValue = 0.0;

    for (int i = -(KernelRadius - 1); i < KernelRadius; i++)
    {
        outputValue += GaussianKernel[abs(i)] * texture(Image, TexCoord + vec2(0, float(i))).r;
    }

    Color = vec4(outputValue, outputValue, outputValue, 1.0);
}
