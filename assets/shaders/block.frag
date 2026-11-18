#version 330 core
// ---------------------------------------------------------------------------
// Runs once per pixel covered by a triangle. There are far more fragments
// than vertices, so cheap is good: one texture read, two multiplies, a mix.
// ---------------------------------------------------------------------------
in vec2 vUv;
in float vShade;
in float vViewDistance;

uniform sampler2D uAtlas;
uniform vec3 uFogColour;
uniform float uFogStart;
uniform float uFogEnd;
uniform float uAlphaCutoff;

out vec4 fragColour;

void main() {
    vec4 texel = texture(uAtlas, vUv);

    // ALPHA CUTOUT: leaves and glass have fully transparent pixels. `discard`
    // throws the fragment away entirely - no colour, no depth - which is how
    // you get see-through holes while still being able to render in the
    // opaque pass (real blending would need sorting). uAlphaCutoff is 0 for
    // the water pass, which wants smooth blending instead.
    if (texel.a < uAlphaCutoff) discard;

    vec3 colour = texel.rgb * vShade;

    // DISTANCE FOG: fade to the sky colour towards the render distance. This
    // is not decoration - it hides the hard edge where chunks stop existing.
    float fog = clamp((vViewDistance - uFogStart) / max(uFogEnd - uFogStart, 0.001), 0.0, 1.0);
    colour = mix(colour, uFogColour, fog);

    fragColour = vec4(colour, texel.a);
}
