#version 330 core

out vec4 Color;

const int DASH_LEN = 3;

void main()
{
    int xdiv = (int(gl_FragCoord.x) / DASH_LEN) % 2;
    int ydiv = (int(gl_FragCoord.y) / DASH_LEN) % 2;
    float outlineColor = (xdiv != ydiv) ? 1.0 : 0.0;

    Color = vec4(outlineColor, outlineColor, outlineColor, 1.0);
}
