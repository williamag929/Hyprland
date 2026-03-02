#version 300 es
// SPATIAL OS: Depth Spatial Shader
// Version: 0.1.0
//
// Purpose:
// Depth shader for Spatial OS implementing adaptive Gaussian blur and
// Z-distance fade. Each window is rendered with:
//   - Reduced opacity proportional to camera distance
//   - Gaussian blur proportional to depth
//   - Automatic perspective scaling
//
// Uniforms:
//   u_zDepth    (float)     : 0.0 = front (sharp), 1.0 = back (very blurry)
//   u_blurRadius (float)   : blur radius in pixels
//   alpha       (float)    : base window opacity (Hyprland standard)
//   tex         (sampler2D): window texture
//   fullSize    (vec2)     : screen resolution (Hyprland standard)

precision highp float;
in vec2 v_texcoord;

uniform sampler2D tex;  // window texture
uniform float u_zDepth;      // normalized: 0.0 = front, 1.0 = back
uniform float u_blurRadius;  // blur radius in pixels
uniform float alpha;         // base window alpha (Hyprland standard)
uniform vec2 fullSize;       // screen resolution (Hyprland standard)

layout(location = 0) out vec4 fragColor;

// ──────────────────────────────────────────────────────────────────────────────
// Adaptive 9-tap Gaussian blur
// ──────────────────────────────────────────────────────────────────────────────
vec4 sampleBlur(sampler2D colorTex, vec2 uv, float radius) {
    // Skip blur when radius is negligible
    if (radius < 0.5) {
        return texture(colorTex, uv);
    }

    // 3x3 Gaussian kernel
    vec4 result = vec4(0.0);
    float totalWeight = 0.0;

    float weights[9] = float[](
        0.0625, 0.125,  0.0625,
        0.125,  0.25,   0.125,
        0.0625, 0.125,  0.0625
    );

    vec2 offsets[9] = vec2[](
        vec2(-1, -1), vec2(0, -1), vec2(1, -1),
        vec2(-1,  0), vec2(0,  0), vec2(1,  0),
        vec2(-1,  1), vec2(0,  1), vec2(1,  1)
    );

    vec2 texel = (radius / 5.0) / fullSize;

    for (int i = 0; i < 9; i++) {
        vec2 sampleUv = uv + offsets[i] * texel;
        result += texture(colorTex, sampleUv) * weights[i];
    }

    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// Depth fade
// ──────────────────────────────────────────────────────────────────────────────
float getDepthFade(float zDepth) {
    // Smooth fade: 0.0 = opaque, 1.0 = fully transparent
    // Cubic curve for a more natural feel
    float t = zDepth * zDepth * zDepth;
    return smoothstep(0.0, 1.0, t);
}

void main() {
    // Sample color with blur
    vec4 color = sampleBlur(tex, v_texcoord, u_blurRadius);

    // Apply depth-based opacity
    float depthFade = getDepthFade(u_zDepth);
    color.a = mix(color.a, 0.0, depthFade);

    // Apply base alpha (Hyprland standard, premultiplied pipeline)
    color *= alpha;

    fragColor = color;
}
