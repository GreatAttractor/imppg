#version 330 core

/// Logical image coordinates including scaling: (0, 0) to (img_w * zoom, img_h * zoom).
layout(location = 0) in vec2 Position;
layout(location = 1) in vec2 in_TexCoord;

out vec2 TexCoord;

/// Scrolled position of the image view area.
uniform ivec2 ScrollPos;
uniform ivec2 ViewportSize;

void main()
{
    gl_Position = vec4(
        2.0 / ViewportSize.x * Position.x - 1.0 - 2.0 * ScrollPos.x / ViewportSize.x,
        -2.0 / ViewportSize.y * Position.y + 1.0 + 2.0 * ScrollPos.y / ViewportSize.y,
        0.0,
        1.0
    );

    TexCoord.xy = in_TexCoord.xy;
}
