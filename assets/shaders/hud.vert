#version 330 core
// 2D overlay: positions arrive in PIXELS and the orthographic matrix maps
// them to clip space. No view matrix, no perspective.
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aUv;
layout(location = 2) in vec4 aColour;

uniform mat4 uProjection;

out vec2 vUv;
out vec4 vColour;

void main() {
    vUv = aUv;
    vColour = aColour;
    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);
}
