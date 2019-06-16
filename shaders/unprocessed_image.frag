#version 330 core

in vec2 TexCoord;
out vec4 Color;

uniform sampler2DRect Image;

void main()
{
    float value = texture(Image, TexCoord).r;
    Color = vec4(value, value, value, 1.0);
}
