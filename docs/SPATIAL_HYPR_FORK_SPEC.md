# SPATIAL-HYPR — Fork de Hyprland para Spatial OS
> Especificación de modificaciones al compositor Hyprland  
> para implementar navegación espacial en eje Z  
> Versión: 0.1.0 | Estado: BORRADOR  
> Agentes: `@architect` `@refactor`  
> Base: Hyprland v0.45.x  
> Conectado con: `SPATIAL_OS_SPEC.md`, `NO_FORMS_SPEC.md`

---

## Índice

1. [Estrategia del Fork](#1-estrategia-del-fork)
2. [Mapa de Archivos a Modificar](#2-mapa-de-archivos-a-modificar)
3. [Fase 1 — Estructura de Datos Z](#3-fase-1--estructura-de-datos-z)
4. [Fase 2 — Renderizado con Perspectiva](#4-fase-2--renderizado-con-perspectiva)
5. [Fase 3 — Shaders de Profundidad](#5-fase-3--shaders-de-profundidad)
6. [Fase 4 — Navegación en Z](#6-fase-4--navegación-en-z)
7. [Fase 5 — Configuración Spatial](#7-fase-5--configuración-spatial)
8. [Fase 6 — ZSpaceManager](#8-fase-6--zspacemanager)
9. [Sincronización con Upstream](#9-sincronización-con-upstream)
10. [Testing y Validación](#10-testing-y-validación)
11. [Tareas por Agente](#11-tareas-por-agente)
12. [Contratos de Interfaz](#12-contratos-de-interfaz)

---

## 1. Estrategia del Fork

### Por qué fork y no plugin

Hyprland tiene un sistema de plugins en C++ (`APICALL`, `HyprlandAPI`). Sin embargo, las modificaciones necesarias para el eje Z tocan el núcleo del renderer y la estructura `CWindow` — capas que la API de plugins no expone. Un fork es la única opción para el nivel de control requerido.

### Principio de mínima divergencia

Cada modificación sigue esta regla: **tocar el mínimo código upstream posible**. Las features nuevas van en archivos nuevos siempre que sea factible. Esto reduce el costo de sincronización con upstream Hyprland.

```
Archivos modificados (upstream)  → mínimos, cambios quirúrgicos
Archivos nuevos (spatial)        → toda la lógica nueva aquí
```

### Estructura del repositorio

```
spatial-hypr/
├── src/
│   ├── spatial/                    ← NUEVO — todo el código spatial aquí
│   │   ├── ZSpaceManager.hpp
│   │   ├── ZSpaceManager.cpp
│   │   ├── SpatialConfig.hpp
│   │   ├── SpatialConfig.cpp
│   │   ├── SpatialInputHandler.hpp
│   │   └── SpatialInputHandler.cpp
│   │
│   ├── render/
│   │   ├── Renderer.cpp            ← MODIFICADO (perspectiva 3D)
│   │   └── shaders/
│   │       ├── depth_spatial.frag  ← NUEVO
│   │       ├── depth_dof.frag      ← NUEVO
│   │       └── passthrough_ar.frag ← NUEVO (futuro AR)
│   │
│   ├── Window.hpp                  ← MODIFICADO (+Z fields)
│   ├── Window.cpp                  ← MODIFICADO (Z init)
│   └── managers/input/
│       └── InputManager.cpp        ← MODIFICADO (scroll → Z nav)
│
├── hyprland.conf.example           ← MODIFICADO (sección [spatial])
├── SPATIAL_CHANGES.md              ← NUEVO — changelog del fork
└── UPSTREAM_SYNC.md                ← NUEVO — guía de rebase
```

---

## 2. Mapa de Archivos a Modificar

Clasificación por impacto y riesgo de conflicto con upstream:

```
IMPACTO ALTO / RIESGO ALTO (tocar con cuidado)
  src/render/Renderer.cpp
    → añadir matriz de perspectiva
    → pasar u_zDepth a shaders
    → ordenar ventanas por Z antes de renderizar (depth sorting)

IMPACTO ALTO / RIESGO MEDIO
  src/Window.hpp
    → añadir m_fZPosition, m_iZLayer, m_fZVelocity

IMPACTO MEDIO / RIESGO BAJO
  src/managers/input/InputManager.cpp
    → interceptar scroll sin modificador → navegación Z
    → añadir keybinds spatial_next_layer / spatial_prev_layer

IMPACTO BAJO / RIESGO BAJO
  hyprland.conf
    → nueva sección $spatial con parámetros de configuración

ARCHIVOS NUEVOS (riesgo cero de conflicto)
  src/spatial/*
  src/render/shaders/depth_spatial.frag
  src/render/shaders/depth_dof.frag
```

---

## 3. Fase 1 — Estructura de Datos Z

### 3.1 Modificación de `src/Window.hpp`

Añadir los campos Z al final de la sección de propiedades de `CWindow`, agrupados en un struct propio para minimizar conflictos con upstream:

```cpp
// src/Window.hpp
// Añadir dentro de class CWindow, al final de las propiedades públicas:

// ── SPATIAL OS: Z-Space properties ──────────────────────────
struct SSpatialProps {
    float  fZPosition   = 0.0f;   // posición Z actual (unidades de mundo)
    float  fZTarget     = 0.0f;   // posición Z objetivo (para animación)
    float  fZVelocity   = 0.0f;   // velocidad actual de animación Z
    int    iZLayer      = 0;      // capa discreta (0=frente, N=fondo)
    float  fDepthNorm   = 0.0f;   // 0.0-1.0, normalizado para shaders
    bool   bZPinned     = false;  // true = no se mueve con la cámara
    bool   bZManaged    = true;   // false = la app controla su Z
} m_sSpatialProps;
// ─────────────────────────────────────────────────────────────
```

### 3.2 Constantes del sistema Z

```cpp
// src/spatial/ZSpaceManager.hpp

namespace Spatial {

constexpr int   Z_LAYERS_COUNT  = 4;
constexpr float Z_LAYER_STEP    = 800.0f;   // unidades entre capas
constexpr float Z_NEAR          = 0.1f;
constexpr float Z_FAR           = 10000.0f;
constexpr float Z_FOV_DEGREES   = 60.0f;
constexpr float Z_ANIM_STIFFNESS = 200.0f;  // rigidez del spring
constexpr float Z_ANIM_DAMPING   = 20.0f;   // amortiguación

// Posición Z por capa
constexpr float LAYER_Z_POSITIONS[Z_LAYERS_COUNT] = {
    0.0f,      // Capa 0: Foreground — app activa
   -800.0f,    // Capa 1: Near — apps recientes
  -1600.0f,    // Capa 2: Mid — segundo plano
  -2800.0f,    // Capa 3: Far — sistema / config
};

// Visibilidad por distancia de capa
constexpr float LAYER_OPACITY[Z_LAYERS_COUNT] = {
    1.00f,   // Capa 0: opacidad total
    0.75f,   // Capa 1: ligero fade
    0.40f,   // Capa 2: claramente al fondo
    0.15f,   // Capa 3: casi invisible
};

constexpr float LAYER_BLUR_RADIUS[Z_LAYERS_COUNT] = {
    0.0f,    // Capa 0: sin blur
    1.5f,    // Capa 1: blur sutil
    5.0f,    // Capa 2: blur evidente
   12.0f,    // Capa 3: blur fuerte
};

} // namespace Spatial
```

---

## 4. Fase 2 — Renderizado con Perspectiva

### 4.1 Modificación de `src/render/Renderer.cpp`

Este es el cambio más importante. Implica tres modificaciones quirúrgicas:

**A) Añadir función de proyección perspectiva**

```cpp
// Añadir en Renderer.cpp, antes de renderWindow()

// ── SPATIAL OS: perspective projection ──
glm::mat4 CSpatialRenderer::getSpatialProjection(int w, int h) {
    float aspect = (float)w / (float)h;
    
    // Perspectiva real — ventanas lejanas se ven más pequeñas
    glm::mat4 proj = glm::perspective(
        glm::radians(Spatial::Z_FOV_DEGREES),
        aspect,
        Spatial::Z_NEAR,
        Spatial::Z_FAR
    );
    
    // Cámara — se mueve en Z según la capa activa
    float camZ = -m_pZSpaceManager->getCameraZ();
    glm::mat4 view = glm::lookAt(
        glm::vec3(w / 2.0f, h / 2.0f, camZ + 1200.0f), // posición cámara
        glm::vec3(w / 2.0f, h / 2.0f, camZ),            // mira al centro
        glm::vec3(0.0f, -1.0f, 0.0f)                    // up (Y invertido en pantalla)
    );
    
    return proj * view;
}
// ─────────────────────────────────────────────────────────────

// Modificar la función de transform de ventana existente:
glm::mat4 getWindowTransform(CWindow* pWindow) {
    auto& sp = pWindow->m_sSpatialProps;
    
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(
        pWindow->m_vRealPosition.x,
        pWindow->m_vRealPosition.y,
        sp.fZPosition  // ← eje Z añadido
    ));
    
    return getSpatialProjection(m_iScreenW, m_iScreenH) * model;
}
```

**B) Depth sorting antes de renderizar**

Las ventanas deben dibujarse de atrás hacia adelante (painter's algorithm) para que la transparencia funcione correctamente:

```cpp
// En renderAllWindows() — añadir sort por Z antes del loop

// ── SPATIAL OS: depth sort ──
std::vector<CWindow*> sortedWindows = g_pCompositor->m_vWindows;
std::sort(sortedWindows.begin(), sortedWindows.end(),
    [](CWindow* a, CWindow* b) {
        // De más lejano (Z más negativo) a más cercano (Z=0)
        return a->m_sSpatialProps.fZPosition < b->m_sSpatialProps.fZPosition;
    }
);
// renderizar sortedWindows en lugar de m_vWindows
// ───────────────────────────────────────────────
```

**C) Pasar u_zDepth al shader por ventana**

```cpp
// En la llamada al shader por cada ventana:
glUniform1f(glGetUniformLocation(prog, "u_zDepth"),
    pWindow->m_sSpatialProps.fDepthNorm);

glUniform1f(glGetUniformLocation(prog, "u_blurRadius"),
    Spatial::LAYER_BLUR_RADIUS[pWindow->m_sSpatialProps.iZLayer]);
```

---

## 5. Fase 3 — Shaders de Profundidad

### 5.1 `depth_spatial.frag` — shader principal

```glsl
// src/render/shaders/depth_spatial.frag
// Shader de profundidad para Spatial OS
// Extiende el blur gaussiano de Hyprland con efectos de profundidad Z

#version 430 core

uniform sampler2D tex;          // textura de la ventana
uniform sampler2D texBlur;      // versión pre-blureada (framebuffer Hyprland)
uniform float u_zDepth;         // 0.0 = frente, 1.0 = fondo
uniform float u_blurRadius;     // radio de blur en px para esta capa
uniform float u_alpha;          // alpha base de la ventana (hyprland)
uniform vec2  u_resolution;     // resolución de pantalla

in  vec2 v_texCoord;
out vec4 fragColor;

// Blur gaussiano 9-tap adaptativo
vec4 sampleBlur(sampler2D t, vec2 uv, float radius) {
    if (radius < 0.5) return texture(t, uv);
    
    vec2 texel = radius / u_resolution;
    vec4 result = vec4(0.0);
    
    // Kernel gaussiano 3x3 con pesos
    float weights[9] = float[](
        0.0625, 0.125, 0.0625,
        0.125,  0.25,  0.125,
        0.0625, 0.125, 0.0625
    );
    vec2 offsets[9] = vec2[](
        vec2(-1,-1), vec2(0,-1), vec2(1,-1),
        vec2(-1, 0), vec2(0, 0), vec2(1, 0),
        vec2(-1, 1), vec2(0, 1), vec2(1, 1)
    );
    
    for (int i = 0; i < 9; i++) {
        result += texture(t, uv + offsets[i] * texel) * weights[i];
    }
    return result;
}

void main() {
    // Curva de profundidad — no lineal, más impacto en capas medias
    float depth = smoothstep(0.0, 1.0, u_zDepth);
    
    // Mezcla nítido ↔ borroso según profundidad (depth of field)
    vec4 sharp  = texture(tex, v_texCoord);
    vec4 blurry = sampleBlur(tex, v_texCoord, u_blurRadius);
    vec4 color  = mix(sharp, blurry, depth * 0.9);
    
    // Tinte atmosférico — lo lejano se enfría visualmente
    // Imita cómo la atmósfera desatura objetos distantes
    vec3 tint = mix(
        vec3(1.0, 1.0, 1.0),          // cercano: color puro
        vec3(0.65, 0.80, 1.0),        // lejano: tinte azul-gris frío
        depth * 0.45
    );
    
    // Opacidad por profundidad
    float alpha = u_alpha * mix(1.0, 0.12, depth * 0.92);
    
    // Viñeta sutil en capas de fondo
    vec2 center = v_texCoord - 0.5;
    float vignette = 1.0 - dot(center, center) * depth * 1.2;
    vignette = clamp(vignette, 0.0, 1.0);
    
    fragColor = vec4(color.rgb * tint * vignette, color.a * alpha);
}
```

### 5.2 `depth_dof.frag` — depth of field cinematográfico

```glsl
// src/render/shaders/depth_dof.frag
// Depth of field con bokeh circular — efecto cinematográfico
// Más costoso, activable por config: spatial_dof = true

#version 430 core

uniform sampler2D tex;
uniform float u_zDepth;
uniform float u_focalPlane;   // 0.0-1.0, dónde está el foco
uniform float u_aperture;     // 0.0-1.0, cuánto bokeh (f/stop)
uniform vec2  u_resolution;

in  vec2 v_texCoord;
out vec4 fragColor;

// Círculo de confusión (CoC) — base del depth of field fotográfico
float circleOfConfusion(float depth, float focal) {
    return abs(depth - focal) * u_aperture * 12.0;
}

vec4 bokehSample(sampler2D t, vec2 uv, float coc) {
    if (coc < 0.5) return texture(t, uv);
    
    vec2 texelSize = coc / u_resolution;
    vec4 result = vec4(0.0);
    float total  = 0.0;
    
    // Muestreo circular (bokeh real)
    int samples = 16;
    for (int i = 0; i < samples; i++) {
        float angle  = float(i) / float(samples) * 6.28318;
        float radius = sqrt(float(i) / float(samples));
        vec2  offset = vec2(cos(angle), sin(angle)) * radius;
        
        vec4 s = texture(t, uv + offset * texelSize);
        // Brillo mayor = más peso en bokeh (simula highlights)
        float w = 1.0 + dot(s.rgb, vec3(0.3, 0.59, 0.11)) * 2.0;
        result += s * w;
        total  += w;
    }
    
    return result / total;
}

void main() {
    float coc = circleOfConfusion(u_zDepth, u_focalPlane);
    fragColor = bokehSample(tex, v_texCoord, coc);
}
```

### 5.3 `passthrough_ar.frag` — mezcla AR (futuro)

```glsl
// src/render/shaders/passthrough_ar.frag
// Mezcla ventanas Wayland con imagen de cámara AR
// Activado cuando spatial_xr_bridge detecta un headset AR

#version 430 core

uniform sampler2D texWindow;     // ventana Wayland renderizada
uniform sampler2D texCamera;     // feed de cámara del headset (AR passthrough)
uniform float u_windowAlpha;     // transparencia global de ventanas en AR
uniform float u_zDepth;

in  vec2 v_texCoord;
out vec4 fragColor;

void main() {
    vec4 window = texture(texWindow, v_texCoord);
    vec4 camera = texture(texCamera, v_texCoord);
    
    // Las ventanas se mezclan sobre el mundo real
    // Más profundidad = más transparentes (menos intrusivas)
    float blend = window.a * u_windowAlpha * (1.0 - u_zDepth * 0.5);
    
    fragColor = mix(camera, window, blend);
}
```

---

## 6. Fase 4 — Navegación en Z

### 6.1 Modificación de `src/managers/input/InputManager.cpp`

```cpp
// InputManager.cpp
// Añadir en onAxisEvent() o equivalente de scroll

// ── SPATIAL OS: scroll → navegación Z ──
void CInputManager::handleSpatialScroll(double delta, bool hasModifier) {
    
    // Sin modificador = navegación Z (cambio de paradigma)
    // Con Super = scroll normal de ventana (comportamiento legacy)
    if (!hasModifier) {
        if (delta > 0)
            g_pZSpaceManager->navigateForward();   // profundizar
        else
            g_pZSpaceManager->navigateBackward();  // subir capa
        
        return; // consumir el evento, no llegar a la ventana
    }
    
    // Con modificador Super → scroll pasa a la ventana normalmente
    handleLegacyScroll(delta);
}
// ────────────────────────────────────────

// Añadir en handleKeybinds():
// spatial_next_layer  → g_pZSpaceManager->navigateForward()
// spatial_prev_layer  → g_pZSpaceManager->navigateBackward()
// spatial_reset_view  → g_pZSpaceManager->resetCamera()
// spatial_pin_window  → g_pZSpaceManager->togglePin(activeWindow)
```

### 6.2 Animación de cámara con spring physics

La navegación Z no es instantánea — usa física de resorte para que la sensación sea fluida y "con peso":

```cpp
// src/spatial/ZSpaceManager.cpp

void ZSpaceManager::updateCameraAnimation(float deltaTime) {
    // Spring physics — igual que Hyprland usa para mover ventanas
    // F = -k*x - c*v  (fuerza = resorte - amortiguación)
    
    float displacement = m_fCameraZTarget - m_fCameraZ;
    float springForce  = displacement * Spatial::Z_ANIM_STIFFNESS;
    float dampForce    = m_fCameraZVelocity * Spatial::Z_ANIM_DAMPING;
    
    float acceleration = springForce - dampForce;
    m_fCameraZVelocity += acceleration * deltaTime;
    m_fCameraZ         += m_fCameraZVelocity * deltaTime;
    
    // Actualizar profundidad normalizada de cada ventana
    for (auto& window : g_pCompositor->m_vWindows) {
        float relZ  = window->m_sSpatialProps.fZPosition - m_fCameraZ;
        float normZ = std::clamp(-relZ / (Spatial::Z_LAYER_STEP * 3.0f),
                                  0.0f, 1.0f);
        window->m_sSpatialProps.fDepthNorm = normZ;
        
        // Actualizar capa discreta
        int layer = static_cast<int>(
            std::round(-relZ / Spatial::Z_LAYER_STEP)
        );
        window->m_sSpatialProps.iZLayer = std::clamp(layer, 0,
            Spatial::Z_LAYERS_COUNT - 1);
    }
}

void ZSpaceManager::navigateForward() {
    int newLayer = std::min(m_iCurrentLayer + 1, Spatial::Z_LAYERS_COUNT - 1);
    setTargetLayer(newLayer);
}

void ZSpaceManager::navigateBackward() {
    int newLayer = std::max(m_iCurrentLayer - 1, 0);
    setTargetLayer(newLayer);
}

void ZSpaceManager::setTargetLayer(int layer) {
    m_iCurrentLayer    = layer;
    m_fCameraZTarget   = -Spatial::LAYER_Z_POSITIONS[layer];
}
```

---

## 7. Fase 5 — Configuración Spatial

### 7.1 Nueva sección en `hyprland.conf`

```ini
# ── SPATIAL OS ──────────────────────────────────────
spatial {
    # Activar modo espacial 3D
    enabled = true
    
    # Número de capas Z (2-8)
    z_layers = 4
    
    # Distancia entre capas en unidades de mundo
    z_layer_step = 800
    
    # Campo de visión en grados (45-90)
    fov = 60
    
    # Física de animación
    anim_stiffness = 200
    anim_damping   = 20
    
    # Efectos visuales
    depth_of_field   = false  # bokeh (costoso en GPU)
    atmospheric_tint = true   # tinte azul en profundidad
    depth_vignette   = true   # viñeta en capas lejanas
    
    # Scroll sin modificador → navegar Z
    # true  = scroll navega capas (nuevo paradigma)
    # false = scroll normal, usar keybinds para Z
    scroll_navigates_z = true
    
    # Reglas de capa por defecto por clase de app
    # app_layer = class, layer_number
    app_layer = kitty,       0   # terminal → frente
    app_layer = firefox,     0   # browser → frente
    app_layer = thunar,      1   # archivos → capa 1
    app_layer = thunderbird, 1   # correo → capa 1
    app_layer = pavucontrol, 2   # audio → capa 2
    app_layer = nm-applet,   3   # sistema → fondo
}

# Keybinds de navegación Z
bind = SUPER, period,    spatial_next_layer
bind = SUPER, comma,     spatial_prev_layer
bind = SUPER, backslash, spatial_reset_view
bind = SUPER, p,         spatial_pin_window
bind = SUPER, bracketright, spatial_move_window_forward
bind = SUPER, bracketleft,  spatial_move_window_backward
```

---

## 8. Fase 6 — ZSpaceManager

Componente central que gestiona el estado del espacio Z, la cámara, y los nodos:

```cpp
// src/spatial/ZSpaceManager.hpp

#pragma once
#include <vector>
#include <functional>
#include <glm/glm.hpp>
#include "../Window.hpp"
#include "SpatialConfig.hpp"

class ZSpaceManager {
public:
    ZSpaceManager();
    ~ZSpaceManager() = default;

    // ── Inicialización ──
    void init(int screenW, int screenH);
    void setScreenSize(int w, int h);

    // ── Cámara ──
    float     getCameraZ()         const { return m_fCameraZ; }
    float     getCameraZTarget()   const { return m_fCameraZTarget; }
    int       getCurrentLayer()    const { return m_iCurrentLayer; }
    glm::mat4 getViewMatrix()      const;
    glm::mat4 getProjectionMatrix() const;
    glm::mat4 getViewProjection()  const;

    // ── Navegación ──
    void navigateForward();
    void navigateBackward();
    void setTargetLayer(int layer);
    void resetCamera();

    // ── Gestión de ventanas en Z ──
    void assignWindowToLayer(CWindow* w, int layer);
    void moveWindowForward(CWindow* w);
    void moveWindowBackward(CWindow* w);
    void togglePin(CWindow* w);
    void autoAssignLayer(CWindow* w);  // basado en reglas de config

    // ── Update loop ──
    void update(float deltaTime);

    // ── Callbacks ──
    using LayerChangedCb = std::function<void(int oldLayer, int newLayer)>;
    void onLayerChanged(LayerChangedCb cb) { m_cbLayerChanged = cb; }

    // ── Queries ──
    std::vector<CWindow*> getWindowsInLayer(int layer) const;
    float getWindowDepthNorm(CWindow* w) const;
    bool  isWindowVisible(CWindow* w) const;

private:
    // Estado de cámara
    float m_fCameraZ        = 0.0f;
    float m_fCameraZTarget  = 0.0f;
    float m_fCameraZVelocity = 0.0f;
    int   m_iCurrentLayer   = 0;

    // Pantalla
    int m_iScreenW = 1920;
    int m_iScreenH = 1080;

    // Config
    SpatialConfig* m_pConfig = nullptr;

    // Callbacks
    LayerChangedCb m_cbLayerChanged;

    // Internos
    void updateCameraAnimation(float deltaTime);
    void updateWindowDepths();
    void notifyLayerChanged(int oldLayer, int newLayer);
};

// Singleton global (igual que Hyprland usa g_pCompositor)
inline std::unique_ptr<ZSpaceManager> g_pZSpaceManager;
```

---

## 9. Sincronización con Upstream

Hyprland tiene releases frecuentes. Estrategia para mantener el fork actualizado:

### 9.1 Estructura de branches

```
main              ← fork estable, siempre compila
develop           ← integración de features spatial
upstream-sync     ← branch temporal para rebases

Releases de Hyprland upstream:
  hyprland/v0.45 ← tagged en el fork para referencia
  hyprland/v0.46 ← cuando sale, se hace rebase de spatial sobre esto
```

### 9.2 Script de sincronización

```bash
#!/usr/bin/env bash
# scripts/sync-upstream.sh
# Sincroniza el fork con la versión más reciente de Hyprland

set -e

UPSTREAM_REMOTE="https://github.com/hyprwm/Hyprland.git"
SPATIAL_BRANCH="develop"

echo "→ Fetching upstream Hyprland..."
git remote add upstream $UPSTREAM_REMOTE 2>/dev/null || true
git fetch upstream

echo "→ Checking upstream version..."
UPSTREAM_TAG=$(git describe --tags upstream/main 2>/dev/null || echo "unknown")
echo "   Upstream: $UPSTREAM_TAG"

echo "→ Creating sync branch..."
git checkout -b "upstream-sync-$(date +%Y%m%d)" upstream/main

echo "→ Cherry-picking spatial commits..."
# Los commits spatial están etiquetados con [SPATIAL] en el mensaje
git log $SPATIAL_BRANCH --oneline --grep="\[SPATIAL\]" \
    --format="%H" | tac | xargs git cherry-pick

echo "→ Running build test..."
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j$(nproc)

echo "✓ Sync complete. Review changes and merge to develop."
```

### 9.3 Convención de commits

Todos los commits del fork deben prefijarse para facilitar cherry-pick:

```
[SPATIAL] feat: add m_fZPosition to CWindow
[SPATIAL] feat: perspective projection in Renderer
[SPATIAL] feat: depth_spatial.frag shader
[SPATIAL] fix: depth sorting order for transparency
[SPATIAL] config: add spatial{} section parser
```

Los commits sin `[SPATIAL]` se asumen como cambios upstream y se descartan en la sincronización.

---

## 10. Testing y Validación

### 10.1 Test de compositor anidado (nested Wayland)

El método más seguro para desarrollar — corre el compositor modificado dentro de tu sesión Hyprland normal:

```bash
# Terminal 1 — compilar y correr el fork en modo anidado
cd spatial-hypr
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# El compositor se abre en una ventana de tu sesión actual
# Si explota, tu escritorio sigue funcionando
WAYLAND_DISPLAY=wayland-99 ./build/Hyprland --nested

# Terminal 2 — lanzar apps de test dentro del compositor anidado
WAYLAND_DISPLAY=wayland-99 kitty &
WAYLAND_DISPLAY=wayland-99 firefox &
```

### 10.2 Suite de tests de renderizado

```cpp
// tests/spatial/test_zspace.cpp

TEST(ZSpaceManager, LayerNavigation) {
    ZSpaceManager mgr;
    mgr.init(1920, 1080);
    
    EXPECT_EQ(mgr.getCurrentLayer(), 0);
    
    mgr.navigateForward();
    EXPECT_EQ(mgr.getCurrentLayer(), 1);
    
    mgr.navigateBackward();
    EXPECT_EQ(mgr.getCurrentLayer(), 0);
    
    // No navegar más allá del límite
    mgr.navigateBackward();
    EXPECT_EQ(mgr.getCurrentLayer(), 0);
}

TEST(ZSpaceManager, WindowDepthNorm) {
    ZSpaceManager mgr;
    mgr.init(1920, 1080);
    
    CWindow mockWindow;
    mockWindow.m_sSpatialProps.fZPosition = 0.0f;    // capa 0
    EXPECT_NEAR(mgr.getWindowDepthNorm(&mockWindow), 0.0f, 0.01f);
    
    mockWindow.m_sSpatialProps.fZPosition = -800.0f; // capa 1
    EXPECT_NEAR(mgr.getWindowDepthNorm(&mockWindow), 0.33f, 0.05f);
}

TEST(ZSpaceManager, SpringAnimation) {
    ZSpaceManager mgr;
    mgr.init(1920, 1080);
    mgr.navigateForward(); // target: capa 1
    
    // Simular 500ms de animación
    for (int i = 0; i < 50; i++)
        mgr.update(0.010f);
    
    // La cámara debe haberse acercado al target
    float target = -Spatial::LAYER_Z_POSITIONS[1];
    EXPECT_NEAR(mgr.getCameraZ(), target, 50.0f); // dentro de 50 unidades
}
```

### 10.3 Validación visual — checklist por fase

```markdown
Fase 1 (Estructura Z)
  [ ] CWindow compila sin errores con los nuevos campos
  [ ] m_fZPosition inicializa a 0.0f en nuevas ventanas
  [ ] Las reglas de config asignan capas correctamente

Fase 2 (Perspectiva)
  [ ] Las ventanas se ven con perspectiva al cambiar de capa
  [ ] El depth sorting evita z-fighting en transparencias
  [ ] La cámara se mueve suavemente entre capas

Fase 3 (Shaders)
  [ ] Ventanas en capa 0: nítidas, opacidad 1.0
  [ ] Ventanas en capa 1: blur 1.5px, opacidad 0.75
  [ ] Ventanas en capa 2: blur 5px, opacidad 0.40
  [ ] Tinte azul-frío visible en capas 2 y 3
  [ ] Sin artefactos visuales en bordes de ventanas

Fase 4 (Navegación)
  [ ] Scroll sin modificador → cambia capa Z
  [ ] Scroll con Super → scroll normal en ventana
  [ ] Keybinds SUPER+. y SUPER+, funcionan
  [ ] Animación spring sin overshooting excesivo

Fase 5 (Config)
  [ ] hyprland.conf parsea sección spatial{} sin error
  [ ] app_layer asigna capas por clase correctamente
  [ ] spatial_dof = true activa shader bokeh
```

---

## 11. Tareas por Agente

### `@architect`

```markdown
@architect TASK-SH-001
Revisar el código fuente de Renderer.cpp de Hyprland v0.45
e identificar el punto exacto de inserción para la matriz de
perspectiva. Documentar las funciones existentes que se usan
para transformar coordenadas de ventana a coordenadas de pantalla.
Entregable: mapa de funciones afectadas + decisión de inserción.

@architect TASK-SH-002
Diseñar la estrategia de depth sorting compatible con el sistema
de capas (layers) existente de Hyprland (overlay, top, normal, bottom).
Las capas Hyprland deben coexistir con las capas Z espaciales sin conflicto.
Entregable: diagrama de precedencia de capas + pseudocódigo del sort.

@architect TASK-SH-003
Diseñar el parser de la sección spatial{} para hyprland.conf.
Hyprland usa su propio sistema de configuración (hyprlang).
Documentar cómo añadir una nueva sección sin romper el parser existente.
Entregable: diff mínimo necesario en el parser + definición de todos los campos.

@architect TASK-SH-004
Evaluar el impacto en rendimiento de la perspectiva 3D.
Hyprland optimiza renderizado con damage tracking (solo redibuja zonas
que cambian). La perspectiva 3D puede invalidar esas optimizaciones.
Proponer estrategia para mantener el damage tracking con Z activo.
Entregable: análisis de impacto + propuesta de mitigación.
```

### `@refactor`

```markdown
@refactor TASK-SH-101
Implementar las modificaciones a Window.hpp y Window.cpp.
Añadir SSpatialProps al final de CWindow sin tocar campos existentes.
Inicializar todos los campos en el constructor.
Tests: verificar que las builds de Hyprland existentes siguen pasando.

@refactor TASK-SH-102
Implementar ZSpaceManager.hpp y ZSpaceManager.cpp completos
según el contrato de interfaz de esta especificación.
Incluir todos los tests de test_zspace.cpp.
El manager debe compilar de forma aislada (mock de CWindow para tests).

@refactor TASK-SH-103
Implementar depth_spatial.frag y depth_dof.frag.
Integrarlos en el sistema de shaders de Hyprland.
Validar compilación en GLSL 4.30 core profile.
Test: script que compile los shaders con glslangValidator.

@refactor TASK-SH-104
Implementar la modificación de Renderer.cpp.
Punto A: función getSpatialProjection() con glm::perspective.
Punto B: depth sorting antes del render loop.
Punto C: paso de uniforms u_zDepth y u_blurRadius por ventana.
Correr en modo nested Wayland y capturar screenshot de validación.

@refactor TASK-SH-105
Implementar las modificaciones a InputManager.cpp.
Interceptar scroll sin modificador → ZSpaceManager::navigateForward/Backward.
Registrar los keybinds spatial_* en el sistema de keybinds de Hyprland.
Test: verificar que scroll con Super sigue funcionando normalmente.

@refactor TASK-SH-106
Implementar el script sync-upstream.sh y la configuración de git remotes.
Crear SPATIAL_CHANGES.md con el log inicial de cambios.
Crear UPSTREAM_SYNC.md con instrucciones paso a paso para hacer rebase.
Configurar GitHub Actions para build automático en cada PR.
```

---

## 12. Contratos de Interfaz

### 12.1 Interface pública de ZSpaceManager (resumen)

```cpp
// Lo que el resto del compositor necesita saber de ZSpaceManager

class ZSpaceManager {
public:
    // Renderer lo llama cada frame
    glm::mat4 getViewProjection() const;
    float     getWindowDepthNorm(CWindow*) const;

    // InputManager lo llama en eventos de scroll/keybind
    void navigateForward();
    void navigateBackward();

    // Compositor lo llama al crear/destruir ventanas
    void autoAssignLayer(CWindow*);
    void removeWindow(CWindow*);

    // Main loop lo llama cada tick
    void update(float deltaTime);
};
```

### 12.2 Uniforms del shader por ventana

```cpp
// Contrato entre Renderer.cpp y los shaders spatial
// Estos uniforms DEBEN estar presentes en todos los shaders spatial

struct SpatialShaderUniforms {
    float u_zDepth;        // 0.0-1.0, profundidad normalizada
    float u_blurRadius;    // px de blur para esta capa
    float u_alpha;         // opacidad calculada por Z
    vec2  u_resolution;    // resolución de pantalla
    // + uniforms estándar de Hyprland (proj, tex, etc.)
};
```

### 12.3 GitHub Actions — CI mínimo

```yaml
# .github/workflows/spatial-build.yml
name: Spatial Hypr Build

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    container: archlinux:latest

    steps:
      - uses: actions/checkout@v4
        with: { submodules: recursive }

      - name: Install deps
        run: |
          pacman -Sy --noconfirm \
            wlroots0.17 wayland wayland-protocols \
            cmake meson clang glm glslang \
            gtest

      - name: Build
        run: |
          cmake -B build -DCMAKE_BUILD_TYPE=Debug \
                -DCMAKE_CXX_COMPILER=clang++
          cmake --build build -j$(nproc)

      - name: Validate shaders
        run: |
          glslangValidator -V src/render/shaders/depth_spatial.frag
          glslangValidator -V src/render/shaders/depth_dof.frag

      - name: Run spatial tests
        run: ctest --test-dir build -R "spatial" --output-on-failure
```

---

## Hoja de Ruta Resumida

```
Semana 1   TASK-SH-001 + TASK-SH-101
           Entorno listo, Window.hpp modificado, compila sin errores

Semana 2   TASK-SH-102
           ZSpaceManager funcional con tests pasando

Semana 3   TASK-SH-103 + TASK-SH-104 parcial
           Shaders compilando, perspectiva visible en modo nested

Semana 4   TASK-SH-104 completo + TASK-SH-105
           Navegación Z funcional con scroll y keybinds

Semana 5   TASK-SH-003 + TASK-SH-103 (config)
           hyprland.conf con sección spatial parseada

Semana 6   TASK-SH-106 + TASK-SH-002
           CI/CD en GitHub Actions, sync script listo

── DEMO GRABABLE ──
           Escritorio 3D con navegación Z, blur por profundidad,
           tinte atmosférico — diferente a todo lo existente en Linux
```

---

## Referencias

- [Hyprland source](https://github.com/hyprwm/Hyprland) — base del fork
- [wlroots renderer](https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/render) — referencia de renderizado
- [glm docs](https://glm.g-truc.net) — matemáticas 3D, `glm::perspective`
- [glslangValidator](https://github.com/KhronosGroup/glslang) — validador de shaders
- [Hyprland wiki — plugins](https://wiki.hyprland.org/Plugins/Development) — contexto del sistema de plugins
- [Learn OpenGL — depth testing](https://learnopengl.com/Advanced-OpenGL/Depth-testing)
- [Learn OpenGL — depth of field](https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom)

---

*Fork spec — Spatial OS / spatial-hypr*  
*Conectado con: `SPATIAL_OS_SPEC.md`, `NO_FORMS_SPEC.md`*  
*Última actualización: Febrero 2026*
