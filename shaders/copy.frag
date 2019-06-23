#version 330 core

in vec2 TexCoord;
out vec4 Color;

uniform sampler2DRect Image;

void main()
{
    Color = texture(Image, TexCoord);
}
