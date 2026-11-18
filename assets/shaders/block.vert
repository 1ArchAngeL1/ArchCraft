#version 330 core
// ---------------------------------------------------------------------------
// Runs once per vertex. Its job: turn a position into a clip-space coordinate.
//
// The chain is  clip = projection * view * world, where "world" here is just
// the chunk-local position plus the chunk's origin. Per-vertex matrix maths is
// the GPU's bread and butter: thousands of these run in parallel.
// ---------------------------------------------------------------------------

// Attributes - per-vertex data, laid out by Mesh::upload.
layout(location = 0) in vec3 aPosition;  // chunk-local
layout(location = 1) in vec2 aUv;        // into the texture atlas
layout(location = 2) in float aShade;    // baked face light x ambient occlusion

// Uniforms - constant for the whole draw call.
uniform mat4 uProjection;
uniform mat4 uView;
uniform vec3 uChunkOrigin;
uniform vec3 uCameraPos;

// Outputs get interpolated across each triangle before reaching the fragment
// shader. Three corner values become a smooth gradient for free.
out vec2 vUv;
out float vShade;
out float vViewDistance;

void main() {
    vec3 worldPos = aPosition + uChunkOrigin;

    vUv = aUv;
    vShade = aShade;
    // Horizontal distance only: looking up should not fog the sky, and it
    // keeps the fog ring circular around the player rather than spherical.
    vViewDistance = length(worldPos.xz - uCameraPos.xz);

    gl_Position = uProjection * uView * vec4(worldPos, 1.0);
}
