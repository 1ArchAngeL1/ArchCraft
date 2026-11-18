#version 330 core
in vec2 vUv;
in vec4 vColour;

uniform sampler2D uAtlas;
uniform int uUseTexture;

out vec4 fragColour;

void main() {
    if (uUseTexture == 1) {
        vec4 texel = texture(uAtlas, vUv);
        if (texel.a < 0.02) discard;
        // Block icons are drawn at full brightness; vColour tints them.
        fragColour = vec4(texel.rgb * vColour.rgb, texel.a * vColour.a);
    } else {
        fragColour = vColour;
    }
}
