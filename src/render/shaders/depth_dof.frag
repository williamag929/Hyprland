// SPATIAL OS: Depth of Field Shader
// Version: 0.1.0
//
// Purpose:
// Implements a depth-of-field (DoF) effect for Spatial OS.
// Distant windows appear more defocused, simulating a camera with
// limited focal length. More pronounced than depth_spatial.frag,
// using a higher-radius Gaussian blur.
//
// Uniforms:
//   u_zDepth       (float)     : normalized 0.0-1.0 (relative depth)
//   u_focusDistance (float)    : focus plane distance in world units
//   u_blurRadius   (float)     : max blur radius in pixels
//   tex            (sampler2D) : window texture
//   fullSize       (vec2)      : screen resolution (Hyprland standard)

#version 430 core

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in vec4 v_color;

layout(binding = 0) uniform sampler2D tex;
uniform float u_zDepth;           // normalized depth [0.0, 1.0]
uniform float u_focusDistance;    // focus plane distance (world units)
uniform float u_blurRadius;       // max blur radius [1, 20] — matches SHADER_BLUR_RADIUS
uniform vec2 fullSize;            // screen resolution (Hyprland standard)

out vec4 fragColor;

// ──────────────────────────────────────────────────────────────────────────────
// 7-tap Gaussian blur (higher quality, higher cost)
// ──────────────────────────────────────────────────────────────────────────────
vec4 gaussianBlur7(sampler2D tex, vec2 uv, float radius) {
    if (radius < 0.5) {
        return texture(tex, uv);
    }

    vec2 texel = (radius / 3.0) / fullSize;

    // 7-tap 1D Gaussian kernel (σ = 1.5)
    float kernel[7] = float[](
        0.064,   // σ = 1.5
        0.120,
        0.176,
        0.204,
        0.176,
        0.120,
        0.064
    );

    vec4 result = vec4(0.0);

    // Horizontal blur
    for (int i = -3; i <= 3; i++) {
        result += texture(tex, uv + vec2(float(i), 0.0) * texel) * kernel[i + 3];
    }

    // Vertical blur (second pass, averaged)
    for (int i = -3; i <= 3; i++) {
        result += texture(tex, uv + vec2(0.0, float(i)) * texel) * kernel[i + 3];
    }

    return result * 0.5;  // Average both passes
}

// ──────────────────────────────────────────────────────────────────────────────
// Circle of Confusion (CoC)
// Determines how blurred a window appears based on depth
// ──────────────────────────────────────────────────────────────────────────────
float calculateCoC(float zDepth) {
    // CoC grows with distance from focus plane
    // Near: soft defocus  |  Far: pronounced defocus
    return u_blurRadius * zDepth;
}

void main() {
    // Compute circle of confusion
    float coc = calculateCoC(u_zDepth);

    // Sample with variable blur
    vec4 color = gaussianBlur7(tex, v_texCoord, coc);

    // Progressive fade for very distant windows
    float farFade = smoothstep(0.7, 1.0, u_zDepth);
    color.a *= (1.0 - farFade * 0.3);  // max 30% fade at the back

    // Multiply by vertex color
    color *= v_color;

    fragColor = color;
}
