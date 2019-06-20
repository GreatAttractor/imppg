#version 330 core

layout(location = 0) in vec2 Position;

out vec2 TexCoord;

uniform ivec2 ScrollPos;
uniform ivec2 ImageSize;
uniform float ZoomFactor;

void main()
{
    gl_Position = vec4(Position.x, Position.y, 0.0, 1.0);

    // calculation assumes that screen coordinates (in `Position`) of (-1, -1) to (1, 1) span the whole
    // visible portion of the image in the image view widget
    TexCoord.x = ImageSize.x / (2 * ZoomFactor) * Position.x + (2 * ScrollPos.x + ImageSize.x) / (2 * ZoomFactor);
    TexCoord.y = -ImageSize.y / (2 * ZoomFactor) * Position.y + (2 * ScrollPos.y + ImageSize.y) / (2 * ZoomFactor);
}
