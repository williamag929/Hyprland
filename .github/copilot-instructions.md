# Spatial OS — Copilot Agent Instructions
> Archivo de contexto persistente para GitHub Copilot Coding Agent  
> Ubicación en repo: `.github/copilot-instructions.md`  
> Última actualización: Febrero 2026

---

## Visión del Proyecto

**Spatial OS** es un entorno de escritorio para Linux donde la navegación ocurre en tres dimensiones reales. Las aplicaciones flotan en **capas de profundidad Z** — el usuario avanza y retrocede en el espacio para acceder a distintos contextos de trabajo.

Opera en dos modos:
- **Modo escritorio 3D** — compositor Wayland con perspectiva y profundidad en pantalla plana
- **Modo AR/VR** — ventanas proyectadas en el espacio físico via Meta Quest 3, Monado, o WebXR en móvil

El proyecto tiene **tres módulos principales**, cada uno con su spec detallada:

| Módulo | Spec | Descripción |
|--------|------|-------------|
| Compositor + AR/VR | `SPATIAL_OS_SPEC.md` | Arquitectura general del sistema |
| Fork de Hyprland | `SPATIAL_HYPR_FORK_SPEC.md` | Modificaciones al compositor Hyprland para eje Z |
| No-Forms | `NO_FORMS_SPEC.md` | Captura de información sin formularios, via IA |

**Antes de implementar cualquier tarea, leer el spec correspondiente completo.**

---

## Stack Tecnológico

### Compositor (C++)
| Componente | Tecnología | Versión mínima |
|---|---|---|
| Base del fork | Hyprland | v0.45.x |
| Protocolo ventanas | Wayland | 1.22 |
| Base compositor | wlroots | 0.17 |
| Renderizado | OpenGL | 4.3 |
| Alternativa GPU | Vulkan | 1.3 |
| XR estándar | OpenXR | 1.0 |
| Runtime XR Linux | Monado | 23.0 |
| Lenguaje principal | C++20 | GCC 13 / Clang 16 |
| Build | CMake + Meson | 3.25 |
| Shaders | GLSL | 4.30 |
| Matemáticas 3D | glm | latest |
| Logging | spdlog | latest |
| Config JSON | nlohmann/json | latest |
| Tests | Google Test | latest |

### Daemon No-Forms (Rust)
| Componente | Tecnología |
|---|---|
| Lenguaje | Rust 2021 edition |
| Base de datos | SurrealDB embebido |
| LLM local (default) | ollama (llama3 / mistral) |
| LLM embebido | llama.cpp |
| LLM cloud (opcional) | Anthropic API — Claude Sonnet |
| Voz / STT | whisper.cpp |
| OCR | Tesseract 5 |
| IPC | Unix socket + JSON |

### WebXR Bridge (Node.js)
| Componente | Tecnología |
|---|---|
| Runtime | Node.js 22 LTS |
| WebSocket | ws ^8.0 |
| WebRTC | wrtc ^0.4 |
| 3D en browser | three.js |

---

## Estructura del Repositorio

### Este repositorio: `spatial-hypr/` (fork de Hyprland — compositor principal)

```
spatial-hypr/                        ← fork de Hyprland v0.45.x (compositor)
├── .github/
│   ├── copilot-instructions.md      ← ESTE ARCHIVO
│   └── workflows/
│       └── spatial-build.yml        ← CI/CD con GitHub Actions
├── src/
│   ├── spatial/                     ← código nuevo (sin conflicto upstream)
│   │   ├── ZSpaceManager.hpp        ← gestor de capas Z y cámara
│   │   ├── ZSpaceManager.cpp
│   │   ├── SpatialConfig.hpp        ← configuración de profundidad
│   │   ├── SpatialConfig.cpp
│   │   ├── SpatialInputHandler.hpp  ← scroll/keybinds para Z nav
│   │   └── SpatialInputHandler.cpp
│   ├── render/
│   │   ├── Renderer.cpp             ← MODIFICADO (perspectiva 3D + depth sort)
│   │   └── shaders/
│   │       ├── depth_spatial.frag   ← NUEVO (blur + fade por Z)
│   │       ├── depth_dof.frag       ← NUEVO (depth of field)
│   │       └── passthrough_ar.frag  ← NUEVO (mezcla AR — futuro)
│   ├── Window.hpp                   ← MODIFICADO (+SSpatialProps struct)
│   ├── Window.cpp                   ← MODIFICADO (init de Z)
│   └── managers/input/
│       └── InputManager.cpp         ← MODIFICADO (scroll → Z nav)
├── docs/
│   ├── SPATIAL_OS_SPEC.md           ← especificación de sistema general
│   ├── SPATIAL_HYPR_FORK_SPEC.md    ← guía de modificaciones de este fork
│   ├── NO_FORMS_SPEC.md             ← referencia (implementado en repo hermano)
│   └── UPSTREAM_SYNC.md             ← guía de sincronización con Hyprland upstream
└── CMakeLists.txt                   ← MODIFICADO (targets spatial/ + shaders)
```

### Repositorios hermanos (desarrollo paralelo):

```
spatial-no-forms/                    ← daemon Rust de normalización IA
  src/
    ├── main.rs / daemon.rs
    ├── normalizer.rs / entity_resolver.rs
    ├── backends/ (ollama, llamacpp, anthropic)
    └── store/ (surrealdb)
  Cargo.toml
  
spatial-shell/                       ← HUD, lanzador, notificaciones del sistema
  src/ (C++ o análogo)
  CMakeLists.txt / Makefile
  
spatial-xr-bridge/                   ← bridge OpenXR → WebXR (Node.js)
  src/ (Node.js / TypeScript)
  package.json
```

**Conectividad entre repos:**
- `spatial-hypr` (compositor) → comunica con `spatial-no-forms` vía IPC (Unix socket + JSON)
- `spatial-hypr` → comunica con `spatial-shell` vía Wayland protocol `spatial-shell-v1`
- `spatial-hypr` → opcional: comunica con `spatial-xr-bridge` vía OpenXR

---

## Reglas de Código

### C++ (compositor + spatial-hypr)
- Estándar: **C++20** obligatorio
- Compilador: **clang++** preferido, GCC 13 como alternativa
- Sin warnings en `clang-tidy` nivel strict
- Toda función y clase pública lleva **documentación Doxygen**
- Tests unitarios con **Google Test** para todo código en `src/spatial/`
- Cobertura mínima: **70%** en `libspatial-core`
- Sin memory leaks — valgrind clean en la suite completa
- Los commits que tocan archivos upstream de Hyprland deben prefijarse con `[SPATIAL]`

```cpp
// Ejemplo de estilo correcto
/// @brief Asigna una ventana a una capa Z específica
/// @param window Puntero a la ventana a asignar
/// @param layer  Índice de capa (0 = frente, N = fondo)
/// @note Thread-safe — usa mutex interno
void ZSpaceManager::assignWindowToLayer(CWindow* window, int layer);
```

### Rust (no-forms-daemon)
- Edición **2021**
- `cargo clippy -- -D warnings` debe pasar sin errores
- `cargo fmt` antes de cada commit
- Manejo de errores con `anyhow` o `thiserror`, nunca `.unwrap()` en código de producción
- Tests de integración con mocks del backend IA

### GLSL (shaders)
- Version `#version 430 core` in all shaders
- Validate with `glslangValidator -G` before each commit (`-G` = OpenGL SPIR-V target; these shaders run through wlroots/EGL, not Vulkan)
- Name uniforms with prefix `u_`, varyings with `v_`

### Commits
- Commits que modifican Hyprland upstream: prefijo `[SPATIAL]`
- Commits de No-Forms: prefijo `[NO-FORMS]`
- Commits del fork base: prefijo `[FORK]`
- Commits de CI/docs: prefijo `[CI]` / `[DOCS]`

---

## Agentes y Responsabilidades

El proyecto usa dos roles de agente:

**`@architect`** — decisiones de diseño de alto nivel, definición de interfaces entre componentes, protocolos, y coherencia arquitectónica. No implementa código de producción directamente — genera specs, headers `.hpp`, y definiciones de protocolo.

**`@refactor`** — implementación de código, tests, integración de componentes, y calidad. Trabaja sobre los diseños entregados por `@architect`.

### Dependencias entre tareas — SPATIAL-HYPR (este repo)

```
@architect TASK-SH-001 (análisis Renderer.cpp)
    └── @refactor TASK-SH-104 (modificar Renderer.cpp)

@architect TASK-SH-002 (depth sorting)
    └── @refactor TASK-SH-104 (integrar en Renderer.cpp)

@architect TASK-SH-003 (parser spatial{})
    └── @refactor (integración en SpatialConfig.cpp)

@architect TASK-SH-004 (damage tracking)
    └── @refactor TASK-SH-102 (considerar en ZSpaceManager)
```

### Dependencias entre repos

```
spatial-hypr (compositor ← ESTE REPO)
    └── expone protocolo spatial-shell-v1
            └── spatial-shell (HUD) — consume protocol
            └── spatial-no-forms — comunica vía IPC socket
            └── spatial-xr-bridge — comunica vía OpenXR
```

**Regla:** nunca empezar una tarea `@refactor` si su `@architect` dependiente no tiene spec aprobada.

---

## Tareas Pendientes Completas

### Módulo: Compositor Base — SPATIAL-HYPR (`SPATIAL_OS_SPEC.md` + `SPATIAL_HYPR_FORK_SPEC.md`)

**Prioridad P0 — En este repo**

```
@architect TASK-SH-001  Analizar Renderer.cpp v0.45 — punto de inserción perspectiva
@architect TASK-SH-002  Estrategia depth sorting compatible con capas Hyprland
@architect TASK-SH-003  Parser sección spatial{} en hyprlang
@architect TASK-SH-004  Análisis impacto en damage tracking — propuesta mitigación

@refactor  TASK-SH-101  Window.hpp — añadir SSpatialProps sin romper upstream
@refactor  TASK-SH-102  ZSpaceManager.hpp/cpp completo + tests
@refactor  TASK-SH-103  depth_spatial.frag + depth_dof.frag — compilar + integrar
@refactor  TASK-SH-104  Renderer.cpp — perspectiva + depth sort + uniforms shader
@refactor  TASK-SH-105  InputManager.cpp — scroll → Z nav + keybinds spatial_*
@refactor  TASK-SH-106  sync-upstream.sh + SPATIAL_CHANGES.md + GitHub Actions CI
```

### Módulo: No-Forms (`NO_FORMS_SPEC.md`)

**Prioridad P1 — En repo hermano `spatial-no-forms/`** (Rust)

```
@architect TASK-NF-001  Protocolo IPC daemon ↔ apps (evaluar D-Bus vs socket vs gRPC)
@architect TASK-NF-002  Schema flexible SpatialDocument — tipos + validación + versionado
@architect TASK-NF-003  Algoritmo EntityResolver — deduplicación de nodos por nombre+contexto

@refactor  TASK-NF-101  no-forms-daemon en Rust — IPC + ollama + SurrealDB + eventos
@refactor  TASK-NF-102  Cliente No-Forms para Quick Notes — nodos inferidos en espacio 3D
@refactor  TASK-NF-103  Captura por voz — whisper.cpp + hotword "oye spatial"
```

### Módulo: Spatial Shell (`spatial-shell`)

**Prioridad P2 — En repo hermano `spatial-shell/`** (C++ o equivalente)

```
HUD — reloj, métricas, indicador Z
Lanzador de apps espacial
Gestor de notificaciones
Protocolo spatial-shell-v1 (Wayland)
```

### Módulo: XR Bridge (`spatial-xr-bridge`)

**Prioridad P3 — En repo hermano `spatial-xr-bridge/`** (Node.js)

```
Inicialización OpenXR
Tracking de headset → actualizar cámara compositor
Controllers/manos → eventos Wayland
Passthrough AR (Meta Quest 3)
WebXR para móvil

---

## Capas Z del Sistema

| Capa | Nombre | Uso | Z position | Opacidad | Blur |
|------|--------|-----|-----------|---------|------|
| 0 | Foreground | App activa | 0.0 | 1.00 | 0px |
| 1 | Near | Apps recientes | -800.0 | 0.75 | 1.5px |
| 2 | Mid | Segundo plano | -1600.0 | 0.40 | 5px |
| 3 | Far | Sistema / config | -2800.0 | 0.15 | 12px |
| -1 | Overlay | HUD, notificaciones | siempre encima | 1.00 | 0px |

---

## Apps Predeterminadas del Sistema

Todas implementan **No-Forms** como interfaz primaria de captura. Los formularios son interfaz secundaria opcional.

| App | Z default | No-Forms input |
|-----|-----------|---------------|
| Quick Notes | 0 | texto libre → extrae tareas, personas, fechas |
| Contactos Espaciales | 1 | texto / voz / foto tarjeta → nodo contacto |
| Calendario Espacial | 0-1 | lenguaje natural → evento + participantes |
| Gestor de Gastos | 1 | foto recibo / voz → gasto categorizado |
| Gestor de Proyectos | 1 | descripción libre → proyecto + tareas |
| Búsqueda Espacial | 0 | query natural → nodos por relevancia (Z = relevancia) |

---

## Hardware Soportado

| Hardware | Runtime | Prioridad | Estado |
|----------|---------|-----------|--------|
| Pantalla plana | Wayland nativo | P0 — siempre funcional | activo |
| Meta Quest 3 | OpenXR + ALVR (WiFi) | P1 — AR passthrough color | planificado |
| Monado (Linux) | OpenXR open source | P2 — cualquier headset | planificado |
| Android ARCore | WebXR en navegador | P3 — móvil accesible | prototipo |
| Google Cardboard | WebXR stereoscopia | P4 — bajo coste | futuro |

**GPU recomendada para desarrollo:** AMD (amdgpu open source, sin fricción Wayland).  
**GPU con fricción:** NVIDIA requiere `GBM_BACKEND=nvidia-drm __GLX_VENDOR_LIBRARY_NAME=nvidia`.

---

## Distro de Desarrollo Recomendada

**Arch Linux** — paquetes de wlroots, Hyprland, y herramientas del stack en tiempo real sin compilar desde fuente. `wlroots-git` en AUR para HEAD de desarrollo.

**Fedora 41/42** — alternativa con mayor estabilidad entre actualizaciones.

**No usar:** Ubuntu LTS, Debian stable, Pop!_OS — ciclos de release incompatibles con el stack.

```bash
# Setup completo en Arch
sudo pacman -S wlroots0.17 wayland wayland-protocols \
               cmake meson clang rust cargo \
               openxr glm spdlog nlohmann-json \
               glslang gtest hyprland sway ollama
yay -S whisper-cpp surrealdb-bin
```

---

## Cómo Desarrollar y Probar en Este Repo

El compositor corre en **modo anidado** (nested Wayland) — una ventana dentro de tu sesión actual. Si explota, tu escritorio sigue funcionando.

```bash
# Compilar el fork
cd spatial-hypr
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
cmake --build build -j$(nproc)

# Correr en modo anidado
WAYLAND_DISPLAY=wayland-99 ./build/Hyprland --nested

# Lanzar apps de test dentro del compositor anidado
WAYLAND_DISPLAY=wayland-99 kitty &
WAYLAND_DISPLAY=wayland-99 firefox &

# Validate shaders before commit
glslangValidator -G src/render/shaders/depth_spatial.frag
glslangValidator -G src/render/shaders/depth_dof.frag

# Tests del módulo spatial
ctest --test-dir build -R "spatial" --output-on-failure
```

### Integración con repos hermanos

Cuando trabajes con `spatial-no-forms`, `spatial-shell`, o `spatial-xr-bridge`:

```bash
# IPC socket para no-forms-daemon
# El compositor escucha en: /tmp/spatial-compositor.sock

# Protocolo Wayland spatial-shell-v1
# spatial-shell usa: wl_registry para descubrir spatial_shell_manager

# OpenXR para spatial-xr-bridge
# El compositor expone surface OpenGL para XR compositing
```

---

## CI/CD — GitHub Actions

Cada PR debe pasar el pipeline completo antes de merge. El workflow corre en container Arch Linux:

```yaml
# Validaciones requeridas por PR en spatial-hypr:
# 1. cmake --build sin errores ni warnings
# 2. glslangValidator en todos los shaders nuevos/modificados
# 3. ctest -R "spatial" — suite de tests espaciales
# 4. clang-tidy en archivos C++ modificados
# 5. valgrind —leak-check=full en tests para no memory leaks
```

**Nota:** Tests de Rust (no-forms-daemon) y Node.js (spatial-xr-bridge) se ejecutan en sus propios repos hermanos. Solo este repo (C++ compositor) tiene CI en spatial-hypr.

Los PRs de agentes se auto-mergean si pasan el CI completo y tienen aprobación de al menos un revisor humano.

---

## Sincronización con Upstream Hyprland

Los commits que modifican archivos de Hyprland upstream **deben** prefijarse con `[SPATIAL]` para facilitar cherry-pick durante rebases.

```bash
# Sincronizar con nueva versión de Hyprland
./scripts/sync-upstream.sh

# El script:
# 1. Fetch de upstream/main
# 2. Crea branch upstream-sync-YYYYMMDD
# 3. Cherry-pick de commits [SPATIAL]
# 4. Corre build de validación
# 5. Abre PR para revisión
```

Ver `UPSTREAM_SYNC.md` para instrucciones detalladas paso a paso.

---

## Criterios de Aceptación Globales

### Rendimiento
- 60fps estables en pantalla plana (GPU media: RX 580 / GTX 1060)
- 72fps en Meta Quest 3 via ALVR
- Latencia input → render < 20ms en modo desktop
- Latencia end-to-end compositor → Quest < 30ms en WiFi 5GHz
- Normalización No-Forms (texto → nodo) < 1.5s con backend local
- Uso de memoria base < 200MB sin aplicaciones abiertas

### Compatibilidad
- Cualquier app Wayland corre sin modificaciones
- XWayland para apps X11 legacy
- Funciona en: Arch Linux, Fedora 41, Ubuntu 24.04
- No-Forms funciona 100% offline con backend ollama

### Calidad de Código
- Cobertura de tests ≥ 70% en `libspatial-core` y `spatial-no-forms`
- Sin memory leaks (valgrind clean)
- Doxygen en todas las APIs públicas
- clang-tidy sin warnings en nivel strict
- `cargo clippy -- -D warnings` limpio

### Privacidad (No-Forms)
- Backend local (ollama) es el default — ningún dato sale del equipo sin consentimiento
- Document store cifrado en reposo
- Exportación de datos en JSON estándar disponible en cualquier momento

---

## Referencias Rápidas

| Recurso | URL |
|---------|-----|
| Hyprland source | github.com/hyprwm/Hyprland |
| wlroots wiki | gitlab.freedesktop.org/wlroots/wlroots/-/wikis |
| The Wayland Book | wayland-book.com |
| OpenXR spec | registry.khronos.org/OpenXR |
| Monado | monado.freedesktop.org |
| xrdesktop (referencia) | gitlab.freedesktop.org/xrdesktop |
| ALVR | github.com/alvr-org/ALVR |
| SurrealDB | surrealdb.com |
| Ollama | ollama.ai |
| whisper.cpp | github.com/ggerganov/whisper.cpp |
| Learn OpenGL | learnopengl.com |
| glslangValidator | github.com/KhronosGroup/glslang |

---

*Spatial OS — copilot-instructions.md*  
*Consolida: SPATIAL_OS_SPEC.md · SPATIAL_HYPR_FORK_SPEC.md · NO_FORMS_SPEC.md*  
*Febrero 2026*
