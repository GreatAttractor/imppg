#version 330 core

/// Logical image coordinates ((0, 0) to (img_w, img_h)).
layout(location = 0) in vec2 Position;

out vec2 TexCoord;

/// Scrolled position of the image view area.
uniform ivec2 ScrollPos;
uniform ivec2 ViewportSize;
uniform float ZoomFactor;

void main()
{
    gl_Position = vec4(
        2.0 * ZoomFactor / ViewportSize.x * Position.x - 1.0 - 2.0 * ScrollPos.x / ViewportSize.x,
        -2.0 * ZoomFactor / ViewportSize.y * Position.y + 1.0 + 2.0 * ScrollPos.y / ViewportSize.y,
        0.0,
        1.0
    );

    TexCoord.xy = Position.xy;
}
