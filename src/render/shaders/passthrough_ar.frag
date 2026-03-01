// ════════════════════════════════════════════════════════════════════════════
// SPATIAL OS — AR Passthrough Blend Shader
// TASK-SH-301 | src/render/shaders/passthrough_ar.frag
//
// Composites the Hyprland window stack over a camera passthrough background.
//
// Modes:
//   u_arPassthrough == 0  desktop mode: no-op; returns compositor color unchanged
//                         (zero overhead when AR hardware is absent or disabled)
//   u_arPassthrough == 1  AR mode: alpha-blend compositor over camera feed
//
// Uniforms:
//   u_tex           (sampler2D) : compositor render texture (window stack output)
//   u_arCameraTex   (sampler2D) : camera passthrough feed supplied by XR runtime
//   u_arPassthrough (int)       : 0 = desktop, 1 = AR passthrough active
//   u_arAlpha       (float)     : global blend weight [0.0 = full camera,
//                                 1.0 = full compositor];  default 1.0
//
// Blend equation:
//   alpha = compositor.a * u_arAlpha
//   out   = mix(camera, compositor, alpha)
//
// When compositor pixels are fully transparent the camera shows through,
// making windows appear to float over the real world in passthrough AR.
// ════════════════════════════════════════════════════════════════════════════

#version 430 core

layout(binding = 0) uniform sampler2D u_tex;          // compositor window stack render texture
layout(binding = 1) uniform sampler2D u_arCameraTex;  // XR runtime camera passthrough texture
uniform int       u_arPassthrough;  // 0 = desktop, 1 = AR passthrough active
uniform float     u_arAlpha;        // global blend weight [0.0 – 1.0]

in  vec2 v_texcoord;
out vec4 fragColor;

void main() {
    vec4 compositor = texture(u_tex, v_texcoord);

    // Fast path: AR disabled — return compositor output unchanged.
    // Uniform-driven branch will be statically resolved by most drivers.
    if (u_arPassthrough == 0) {
        fragColor = compositor;
        return;
    }

    vec4 camera = texture(u_arCameraTex, v_texcoord);

    // Alpha-over blend:
    //   compositor alpha=1 → window occludes camera
    //   compositor alpha=0 → camera shows through (AR see-through effect)
    //   u_arAlpha provides a global dimming knob for fade-in / fade-out
    float alpha = compositor.a * clamp(u_arAlpha, 0.0, 1.0);
    fragColor   = mix(camera, compositor, alpha);
}
