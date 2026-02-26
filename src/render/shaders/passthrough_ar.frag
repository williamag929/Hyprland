// SPATIAL OS: Passthrough AR Shader
// Autor: Spatial Compositor Team
// Versión: 0.1.0
//
// Propósito:
// Shader para mezcla de AR passthrough en dispositivos como Meta Quest 3.
// Combina la imagen capturada por las cámaras del headset con las ventanas
// renderizadas del compositor, permitiendo AR semi-transparente.
//
// Esta es una versión placeholder para desarrollo futuro. Actualmente los
// headsets envían la imagen de passthrough a través de OpenXR directamente.
//
// Uniforms (futuro):
//   u_passthroughTex (sampler2D): textura capturada por cámaras del headset
//   u_blendAlpha (float)         : factor de mezcla (0 = solo compositor, 1 = solo AR)
//   u_colorCorrection (mat3)     : matriz de corrección de color

#version 430 core

layout(location = 0) in vec2 v_texCoord;
layout(location = 1) in vec4 v_color;

uniform sampler2D tex;                // Textura renderizada del compositor
uniform sampler2D u_passthroughTex;   // Imagen de passthrough (futuro)
uniform float u_blendAlpha;           // Factor de mezcla [0, 1]
uniform mat3 u_colorCorrection;       // Corrección de color del headset

out vec4 fragColor;

void main() {
    // Versión placeholder: solo renderizar la textura sin modificación
    // En una implementación real, aquí se mezclaría con la imagen de passthrough
    // del headset (Quest 3, Monado, etc.)

    vec4 compositorColor = texture(tex, v_texCoord);

    // TODO: Implementar cuando OpenXR passthrough esté integrado
    // vec4 passthroughColor = texture(u_passthroughTex, v_texCoord);
    // vec4 blended = mix(compositorColor, passthroughColor, u_blendAlpha);
    // fragColor = vec4(u_colorCorrection * blended.rgb, blended.a);

    fragColor = compositorColor * v_color;
}
