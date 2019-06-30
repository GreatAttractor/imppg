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
