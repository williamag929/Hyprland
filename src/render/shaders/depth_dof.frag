// SPATIAL OS: Depth of Field Shader
// Autor: Spatial Compositor Team
// Versión: 0.1.0
//
// Propósito:
// Implementa efecto de profundidad de campo (Depth of Field / DoF)
// para Spatial OS. Las ventanas lejanas aparecen más desenfocadas de forma
// más realista simulando una cámara con distancia focal limitada.
//
// El efecto de DoF es más pronunciado que depth_spatial.frag y usa
// Gaussian blur de mayor radio.
//
// Uniforms:
//   u_zDepth (float)      : normalizado 0.0-1.0 (profundidad relativa)
//   u_focusDistance (float): distancia de enfoque (en unidades)
//   u_blurMaxRadius (float): radio máximo de blur en píxeles
//   tex (sampler2D)       : textura de la ventana

#version 430 core

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in vec4 v_color;

uniform sampler2D tex;
uniform float u_zDepth;           // Profundidad normalizada [0, 1]
uniform float u_focusDistance;    // Distancia de enfoque (centro de atención)
uniform float u_blurRadius;        // Radio máximo de blur [1, 20] — matches SHADER_BLUR_RADIUS
uniform vec2 fullSize;            // Resolución de pantalla (Hyprland standard)

out vec4 fragColor;

// ──────────────────────────────────────────────────────────────────────────────
// Blur Gaussiano 7-tap de mayor radio (más costoso pero mejor calidad)
// ──────────────────────────────────────────────────────────────────────────────
vec4 gaussianBlur7(sampler2D tex, vec2 uv, float radius) {
    if (radius < 0.5) {
        return texture(tex, uv);
    }

    vec2 texel = (radius / 3.0) / fullSize;

    // Kernel Gaussiano 7-tap 1D (horizontal y vertical)
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

    // Blur horizontal
    for (int i = -3; i <= 3; i++) {
        result += texture(tex, uv + vec2(float(i), 0.0) * texel) * kernel[i + 3];
    }

    // Blur vertical (segunda pasada, aproximada con media)
    for (int i = -3; i <= 3; i++) {
        result += texture(tex, uv + vec2(0.0, float(i)) * texel) * kernel[i + 3];
    }

    return result * 0.5;  // Promediar ambas pasadas
}

// ──────────────────────────────────────────────────────────────────────────────
// Cálculo de círculo de confusión (CoC)
// Define qué tan borrosa aparece una ventana según su profundidad
// ──────────────────────────────────────────────────────────────────────────────
float calculateCoC(float zDepth) {
    // CoC crece exponencialmente con la distancia al plano de enfoque
    // Near: desenfoque suave
    // Far: desenfoque pronunciado
    return u_blurRadius * zDepth;
}

void main() {
    // Calcular círculo de confusión
    float coc = calculateCoC(u_zDepth);

    // Muestrear con blur variable
    vec4 color = gaussianBlur7(tex, v_texCoord, coc);

    // Aplicar fade progresivo para ventanas muy lejanas
    float farFade = smoothstep(0.7, 1.0, u_zDepth);
    color.a *= (1.0 - farFade * 0.3);  // Fade máximo 30% en el fondo

    // Multiplicar por color vertex
    color *= v_color;

    fragColor = color;
}
