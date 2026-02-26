// SPATIAL OS: Depth Spatial Shader
// Autor: Spatial Compositor Team
// Versión: 0.1.0
// 
// Propósito:
// Shader de profundidad para Spatial OS que implementa blur adaptativo
// y desvanecimiento por distancia Z. Cada ventana se renderiza con:
// - Opacidad reducida según la distancia a la cámara
// - Blur Gaussiano proporcional a la distancia
// - Escala perspectiva automática
//
// Uniforms:
//   u_zDepth (float)      : 0.0 = frente (nítido), 1.0 = fondo (muy borroso)
//   u_blurRadius (float)  : radio de blur en píxeles
//   u_alpha (float)       : opacidad base de la ventana
//   tex (sampler2D)       : textura de la ventana
//   texBlur (sampler2D)   : versión pre-blureada de la textura

#version 430 core

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in vec4 v_color;

uniform sampler2D tex;       // Textura principal
uniform float u_zDepth;      // Normalizado: 0.0 = frente, 1.0 = fondo
uniform float u_blurRadius;  // Radio de blur en píxeles
uniform float alpha;         // Alpha base de la ventana (Hyprland standard)
uniform vec2 fullSize;       // Resolución de pantalla (Hyprland standard)

out vec4 fragColor;

// ──────────────────────────────────────────────────────────────────────────────
// Blur Gaussiano 9-tap adaptativo
// ──────────────────────────────────────────────────────────────────────────────
vec4 sampleBlur(sampler2D colorTex, vec2 uv, float radius) {
    // Si el radio es muy pequeño, no hacer blur
    if (radius < 0.5) {
        return texture(colorTex, uv);
    }

    // Kernel gaussiano 3x3
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
// Desvanecimiento por profundidad
// ──────────────────────────────────────────────────────────────────────────────
float getDepthFade(float zDepth) {
    // Función de fade suave: 0.0 = opaco, 1.0 = completamente transparente
    // Curva cúbica para fade más natural
    float t = zDepth * zDepth * zDepth;
    return smoothstep(0.0, 1.0, t);
}

void main() {
    // Muestrear color con blur
    vec4 color = sampleBlur(tex, v_texCoord, u_blurRadius);

    // Aplicar opacidad por profundidad
    float depthFade = getDepthFade(u_zDepth);
    color.a = mix(color.a, 0.0, depthFade);

    // Aplicar alpha base (Hyprland standard)
    color.a *= alpha;

    // Multiplicar por color vertex (preserva tinting)
    color *= v_color;

    fragColor = color;
}
