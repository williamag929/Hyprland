# SPATIAL OS — Especificación de Sistema AR/VR para Linux
> Documento de diseño y arquitectura para desarrollo asistido por IA  
> Versión: 0.1.0 | Estado: BORRADOR  
> Agentes: `@architect` `@refactor`

---

## Índice

1. [Visión del Producto](#1-visión-del-producto)
2. [Arquitectura General](#2-arquitectura-general)
3. [Componentes del Sistema](#3-componentes-del-sistema)
4. [Capas de Profundidad Z](#4-capas-de-profundidad-z)
5. [Integración AR/VR con OpenXR](#5-integración-arvr-con-openxr)
6. [Hardware Soportado](#6-hardware-soportado)
7. [Stack Tecnológico](#7-stack-tecnológico)
8. [Tareas por Agente](#8-tareas-por-agente)
9. [Roadmap de Desarrollo](#9-roadmap-de-desarrollo)
10. [Contratos de Interfaz](#10-contratos-de-interfaz)
11. [Criterios de Aceptación](#11-criterios-de-aceptación)

---

## 1. Visión del Producto

**Spatial OS** es un entorno de escritorio para Linux donde la navegación ocurre en tres dimensiones reales. En lugar de menús planos y grillas de iconos, las aplicaciones flotan en **capas de profundidad Z** — el usuario avanza y retrocede en el espacio para acceder a distintos contextos de trabajo.

El sistema opera en dos modos:

- **Modo escritorio 3D** — compositor Wayland con perspectiva y profundidad, pantalla plana convencional
- **Modo AR/VR** — las ventanas se proyectan en el espacio físico a través de headset (Meta Quest 3, gafas AR) o teléfono (WebXR + ARCore)

### Principios de diseño

- **Profundidad = prioridad**: lo urgente está cerca, lo pasivo está lejos
- **Sin menús**: la navegación es espacial, no jerárquica
- **Hardware agnóstico**: OpenXR como capa de abstracción universal
- **Linux nativo**: Wayland, open source, sin dependencias propietarias obligatorias

---

## 2. Arquitectura General

```
┌─────────────────────────────────────────────────────────┐
│                    SPATIAL OS                           │
│                                                         │
│  ┌─────────────────┐     ┌──────────────────────────┐  │
│  │  Spatial Shell  │     │     App Nodes (Z-space)  │  │
│  │  (HUD, lanzador │     │  Layer 0 | Layer 1 | ... │  │
│  │   notificaciones│     │  foreground → background │  │
│  └────────┬────────┘     └────────────┬─────────────┘  │
│           │                           │                 │
│           └──────────┬────────────────┘                 │
│                      ▼                                  │
│         ┌────────────────────────┐                      │
│         │   Spatial Compositor   │                      │
│         │  (wlroots + OpenGL)    │                      │
│         │  Z-camera | shaders    │                      │
│         └────────────┬───────────┘                      │
│                      │                                  │
└──────────────────────┼──────────────────────────────────┘
                       │
          ┌────────────┴────────────┐
          ▼                         ▼
┌──────────────────┐     ┌──────────────────────┐
│  Wayland Display │     │    OpenXR Layer       │
│  (pantalla plana)│     │  (headset / móvil)    │
└──────────────────┘     └──────────┬────────────┘
                                    │
                     ┌──────────────┼──────────────┐
                     ▼              ▼               ▼
               Meta Quest 3    Monado (PC)     WebXR (móvil)
```

---

## 3. Componentes del Sistema

### 3.1 `spatial-compositor`
Compositor Wayland basado en **wlroots**. Gestiona ventanas como objetos 3D con posición `(x, y, z)`, escala y opacidad dependientes de la distancia a la cámara.

**Responsabilidades:**
- Asignar y mantener la posición Z de cada ventana
- Renderizar con perspectiva 3D usando OpenGL
- Aplicar shaders de distancia (blur, fade, scale)
- Exponer protocolo Wayland extendido: `xdg-shell` + `spatial-shell-v1` (protocolo custom)

**Archivos clave:**
```
src/
  compositor/
    SpatialCompositor.cpp     # entrada principal, setup wlroots
    ZSpaceManager.cpp         # gestión de capas Z
    SpatialRenderer.cpp       # renderizado OpenGL con perspectiva
    shaders/
      depth_fade.glsl         # opacidad por distancia
      depth_blur.glsl         # desenfoque por distancia
      passthrough_ar.glsl     # mezcla AR con imagen real
```

### 3.2 `spatial-shell`
La interfaz del usuario — HUD, lanzador de apps, notificaciones. Equivalente a GNOME Shell pero para el espacio 3D.

**Responsabilidades:**
- Renderizar el HUD (reloj, métricas, indicador Z)
- Gestionar el lanzador espacial de aplicaciones
- Manejar gestos de navegación (scroll, swipe, controllers)
- Comunicarse con `spatial-compositor` vía protocolo `spatial-shell-v1`

### 3.3 `spatial-xr-bridge`
Capa de abstracción entre el compositor y OpenXR. Traduce el espacio de ventanas 2.5D a un espacio XR real con tracking de cabeza y manos.

**Responsabilidades:**
- Inicializar sesión OpenXR
- Leer pose del headset → actualizar cámara del compositor
- Mapear controllers/manos → eventos de input Wayland
- Gestionar passthrough AR (cámaras del Quest 3)
- Modo WebXR: serializar frames y enviar al navegador del teléfono

### 3.4 `spatial-protocol`
Definición del protocolo Wayland extendido `spatial-shell-v1`.

```xml
<!-- spatial-shell-v1.xml (fragmento) -->
<interface name="spatial_surface_v1" version="1">
  <request name="set_z_layer">
    <arg name="layer" type="int" summary="profundidad 0=frente, N=fondo"/>
  </request>
  <request name="set_spatial_position">
    <arg name="x" type="fixed"/>
    <arg name="y" type="fixed"/>
    <arg name="z" type="fixed"/>
  </request>
  <event name="z_layer_changed">
    <arg name="new_layer" type="int"/>
  </event>
</interface>
```

---

## 4. Capas de Profundidad Z

El sistema organiza el espacio en **capas discretas** (0 a N) más posicionamiento continuo dentro de cada capa.

### 4.1 Modelo de capas

| Capa | Nombre | Uso | Distancia visual |
|------|--------|-----|-----------------|
| 0 | Foreground | App activa, foco total | 0 — en frente |
| 1 | Near | Apps en uso reciente | ~600px Z |
| 2 | Mid | Apps en segundo plano | ~1200px Z |
| 3 | Far | Sistema, config, IA | ~2000px Z |
| -1 | Overlay | HUD, notificaciones | Siempre encima |

### 4.2 Comportamiento visual por distancia

```
Distancia 0   → opacity: 1.0  | scale: 1.0   | blur: 0px
Distancia 1   → opacity: 0.75 | scale: 0.85  | blur: 1px
Distancia 2   → opacity: 0.45 | scale: 0.65  | blur: 4px
Distancia 3   → opacity: 0.2  | scale: 0.45  | blur: 10px
```

### 4.3 Navegación

- **Scroll / rueda del ratón** → mover cámara en eje Z (avanzar/retroceder capa)
- **Teclas ↑↓ / ArrowUp/Down** → navegación discreta entre capas
- **Drag derecho** → rotación libre del espacio (±25° X, ±30° Y)
- **Pinch gesture** (touch / controllers) → zoom continuo en Z
- **Gaze + dwell** (AR headset) → selección por mirada

---

## 5. Integración AR/VR con OpenXR

### 5.1 Sesión OpenXR

```cpp
// spatial-xr-bridge: inicialización básica
XrInstance instance;
XrSession session;
XrSpace referenceSpace;  // XR_REFERENCE_SPACE_TYPE_LOCAL

// Extensions requeridas
const char* extensions[] = {
  "XR_KHR_opengl_enable",
  "XR_FB_passthrough",          // AR passthrough Meta Quest
  "XR_EXT_hand_tracking",       // tracking de manos
  "XR_KHR_android_surface_swapchain"  // Android/Quest
};
```

### 5.2 Loop de renderizado XR

```
Por cada frame:
  1. xrWaitFrame() → esperar sync del headset
  2. xrBeginFrame()
  3. Leer pose: xrLocateSpace(headSpace, referenceSpace) → viewMatrix
  4. Actualizar cámara del compositor con viewMatrix
  5. Renderizar cada ojo (left/right swapchain)
  6. Composite de ventanas Wayland sobre imagen AR
  7. xrEndFrame() → presentar
```

### 5.3 Modos AR

**Passthrough (Meta Quest 3 / Quest Pro)**
```
xrCreatePassthroughFB()
xrPassthroughLayerCreateFB(XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB)
// Las ventanas se renderizan sobre la imagen real del mundo
```

**WebXR (móvil)**
```javascript
// En el navegador del teléfono
const session = await navigator.xr.requestSession('immersive-ar', {
  requiredFeatures: ['hit-test', 'anchors'],
  optionalFeatures: ['dom-overlay', 'depth-sensing']
});
// El compositor transmite frames via WebSocket o WebRTC
// Las ventanas se anclan al mundo usando hit-test
```

### 5.4 Input en XR

| Fuente | Acción | Resultado |
|--------|--------|-----------|
| Controller trigger | Click en nodo | Activar app |
| Controller thumbstick Y | Scroll | Navegar capa Z |
| Hand pinch | Select | Click en ventana |
| Hand grab + move | Drag | Reubicar ventana en espacio |
| Gaze + 1.5s dwell | Focus | Enfocar ventana |
| Head nod (gesture) | Confirm | Aceptar diálogo |

---

## 6. Hardware Soportado

### Prioridad 1 — Meta Quest 3
- Runtime: OpenXR via Meta XR SDK
- Passthrough: color, 4K por ojo
- Input: controllers Touch Plus + hand tracking
- Conexión al PC Linux: ALVR (WiFi, ~15ms latency) o cable USB

### Prioridad 2 — Monado (Linux nativo)
- Cualquier headset OpenXR compatible (Valve Index, Varjo, etc.)
- Runtime 100% open source
- Sin intermediarios propietarios

### Prioridad 3 — WebXR en Android
- ARCore (Android 7+)
- Google Cardboard / gafas Cardboard (~5€) para stereoscopia
- Streaming desde PC vía WebSocket/WebRTC

### Prioridad 4 — Pantalla plana (modo desktop)
- Sin headset, solo la metáfora 3D en pantalla 2D
- Mouse scroll para navegar Z
- Compatibilidad total con cualquier setup Linux/Wayland

---

## 7. Stack Tecnológico

### Core
| Componente | Tecnología | Versión mínima |
|---|---|---|
| Protocolo de ventanas | Wayland | 1.22 |
| Base compositor | wlroots | 0.17 |
| Renderizado | OpenGL | 4.3 |
| Alternativa GPU moderna | Vulkan | 1.3 |
| XR estándar | OpenXR | 1.0 |
| Runtime XR Linux | Monado | 23.0 |
| Lenguaje principal | C++20 | GCC 13 / Clang 16 |
| Build | CMake + Meson | 3.25 |
| Shaders | GLSL | 4.30 |

### Dependencias adicionales
```cmake
find_package(wlroots REQUIRED)
find_package(OpenXR REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm REQUIRED)          # matemáticas 3D
find_package(nlohmann_json REQUIRED) # config
find_package(spdlog REQUIRED)        # logging
```

### Para WebXR bridge
```json
{
  "dependencies": {
    "ws": "^8.0",
    "wrtc": "^0.4",
    "three": "^0.160"
  }
}
```

---

## 8. Tareas por Agente

### `@architect` — Diseño y estructura

El agente `@architect` es responsable de las decisiones de diseño de alto nivel, la definición de interfaces entre componentes, y la coherencia arquitectónica del sistema.

**Tareas asignadas:**

```markdown
@architect TASK-001
Diseñar el protocolo Wayland extendido `spatial-shell-v1`.
Definir todos los requests, events y enums necesarios para:
- Asignar posición Z a superficies
- Notificar cambios de capa activa
- Registrar apps como "nodos espaciales" con metadata (icono, label, color)
Entregable: archivo XML de protocolo completo + documentación de cada mensaje.

@architect TASK-002
Diseñar la API C++ de ZSpaceManager.
Requisitos:
- Gestionar hasta 8 capas Z
- Soporte para posicionamiento continuo dentro de cada capa
- Eventos cuando la cámara cambia de capa
- Thread-safe (el compositor y el bridge XR corren en hilos distintos)
Entregable: header .hpp con la API pública completa + diagrama de clases.

@architect TASK-003
Diseñar la arquitectura del spatial-xr-bridge.
Debe soportar múltiples backends (Monado, Meta, WebXR) con la misma interfaz.
Usar el patrón Strategy para los backends.
Entregable: diagrama de clases + interfaces abstractas en C++.

@architect TASK-004
Definir el modelo de datos para un "Nodo Espacial" (app en el espacio).
Debe incluir: posición 3D, capa Z, estado (activo/inactivo/oculto),
superficie Wayland asociada, metadata visual.
Entregable: struct/class con serialización JSON para persistencia.

@architect TASK-005
Diseñar el sistema de gestos.
Unificar input de: mouse, teclado, touch, controllers XR, hand tracking.
Abstracción que emita eventos semánticos: "navigate_z(+1)", "select()", "drag(dx,dy,dz)".
Entregable: diagrama de flujo de eventos + clase GestureEngine con API pública.
```

---

### `@refactor` — Implementación y calidad

El agente `@refactor` es responsable de la implementación de código, refactorización para mantener calidad, y asegurar que el código existente cumpla con los estándares del proyecto.

**Tareas asignadas:**

```markdown
@refactor TASK-101
Implementar ZSpaceManager.cpp basado en el diseño de @architect TASK-002.
Requisitos de implementación:
- std::map<int, std::vector<SpatialNode*>> para organizar nodos por capa
- mutex para thread safety
- Función getCameraTransform(float cameraZ) que retorna glm::mat4
- Tests unitarios con Google Test
Código de referencia: src/compositor/ZSpaceManager.hpp (a crear por @architect)

@refactor TASK-102
Implementar los shaders GLSL de profundidad.
Archivos: depth_fade.glsl, depth_blur.glsl
- depth_fade: recibe uniform float u_depth (0.0-1.0), aplica curva de opacidad
- depth_blur: implementar blur gaussiano 9-tap con radio proporcional a u_depth
- Deben compilar en OpenGL 4.3 core profile
Referencia visual: en capa 0 → nítido; en capa 3 → 10px blur + 20% opacidad

@refactor TASK-103
Refactorizar el prototipo HTML/JS existente (spatial-os.html) para:
1. Extraer la lógica de ZSpace a una clase JavaScript pura (sin DOM)
2. Añadir soporte WebXR con fallback a modo 2D
3. Añadir conexión WebSocket para recibir estado del compositor Linux
4. Mantener compatibilidad con el modo standalone actual
Estándar: ES2022, sin dependencias externas excepto three.js para XR

@refactor TASK-104
Implementar el backend WebXR del spatial-xr-bridge.
Node.js + ws para streaming de frames desde el compositor al móvil.
- Protocolo: WebSocket con mensajes JSON para estado + ArrayBuffer para frames
- Framerate objetivo: 60fps en WiFi 5GHz
- Compresión: H.264 via wasm-av1 o mjpeg como fallback

@refactor TASK-105
Crear el CMakeLists.txt completo del proyecto.
Estructura de targets:
  - spatial-compositor (ejecutable)
  - spatial-shell (ejecutable)  
  - spatial-xr-bridge (ejecutable)
  - libspatial-core (shared library con ZSpaceManager, GestureEngine)
  - spatial-protocol (target para generar código Wayland desde XML)
Incluir: tests, instalación, pkg-config

@refactor TASK-106
Implementar el sistema de configuración.
Archivo: ~/.config/spatial-os/config.json
Campos: capas activas, keybindings, hardware preferido, tema visual.
Usar nlohmann/json + validación de schema.
Debe recargarse en caliente (inotify) sin reiniciar el compositor.
```

---

## 9. Roadmap de Desarrollo

### Fase 0 — Fundamentos (Semanas 1-4)
```
[ ] @architect  TASK-001  Protocolo spatial-shell-v1
[ ] @architect  TASK-002  API ZSpaceManager
[ ] @refactor   TASK-105  CMakeLists.txt
[ ] @refactor   TASK-102  Shaders GLSL base
```

### Fase 1 — Compositor mínimo (Semanas 5-10)
```
[ ] @refactor   TASK-101  ZSpaceManager implementado
[ ] @architect  TASK-004  Modelo SpatialNode
[ ] @architect  TASK-005  Sistema de gestos
[ ] Milestone: ventanas Wayland con navegación Z en pantalla plana
```

### Fase 2 — WebXR en móvil (Semanas 11-16)
```
[ ] @refactor   TASK-103  Prototipo HTML → WebXR
[ ] @refactor   TASK-104  WebXR bridge Node.js
[ ] Milestone: desktop Linux visible en teléfono Android con ARCore
```

### Fase 3 — OpenXR nativo (Semanas 17-28)
```
[ ] @architect  TASK-003  Arquitectura XR bridge
[ ] Implementar backend Monado
[ ] Implementar backend Meta Quest (via ALVR)
[ ] @refactor   TASK-106  Sistema de configuración
[ ] Milestone: escritorio 3D en Meta Quest 3 con passthrough AR
```

### Fase 4 — Polish y release (Semanas 29-40)
```
[ ] Hand tracking completo
[ ] Animaciones de transición entre capas
[ ] Soporte multi-monitor en modo desktop
[ ] Empaquetado: AUR (Arch), Flatpak, .deb
[ ] Milestone: v1.0.0 public release
```

---

## 10. Contratos de Interfaz

### 10.1 IXRBackend (interfaz abstracta para backends XR)

```cpp
class IXRBackend {
public:
  virtual ~IXRBackend() = default;

  // Ciclo de vida
  virtual bool initialize() = 0;
  virtual void shutdown() = 0;
  virtual bool isAvailable() const = 0;

  // Frame loop
  virtual XRFrameState waitFrame() = 0;
  virtual void beginFrame(const XRFrameState&) = 0;
  virtual void endFrame(const XRFrameState&, const std::vector<XRLayer>&) = 0;

  // Tracking
  virtual glm::mat4 getHeadPose() const = 0;
  virtual std::optional<HandPose> getHandPose(Hand hand) const = 0;

  // AR
  virtual bool supportsPassthrough() const = 0;
  virtual void setPassthroughEnabled(bool enabled) = 0;

  // Eventos
  virtual void setInputCallback(InputCallback cb) = 0;
};

// Implementaciones concretas
class MonadoBackend    : public IXRBackend { ... };
class MetaQuestBackend : public IXRBackend { ... };
class WebXRBackend     : public IXRBackend { ... };
class NullBackend      : public IXRBackend { ... }; // modo desktop
```

### 10.2 ZSpaceManager API pública

```cpp
class ZSpaceManager {
public:
  // Gestión de nodos
  NodeId addNode(SpatialNode node);
  void   removeNode(NodeId id);
  void   setNodeLayer(NodeId id, int layer);
  void   setNodePosition(NodeId id, glm::vec3 pos);

  // Cámara
  void      setCameraLayer(int layer);
  void      setCameraZ(float z);
  float     getCameraZ() const;
  glm::mat4 getCameraTransform() const;

  // Consultas
  std::vector<NodeId> getNodesInLayer(int layer) const;
  std::vector<NodeId> getVisibleNodes(float maxDistance = 2.0f) const;
  float               getNodeVisibility(NodeId id) const; // 0.0 - 1.0

  // Eventos
  using LayerChangedCb = std::function<void(int oldLayer, int newLayer)>;
  void setLayerChangedCallback(LayerChangedCb cb);
};
```

### 10.3 Protocolo WebSocket (bridge → móvil)

```typescript
// Mensajes del compositor al cliente WebXR

type ServerMessage =
  | { type: 'frame';  data: ArrayBuffer; timestamp: number }
  | { type: 'state';  nodes: SpatialNode[]; cameraZ: number }
  | { type: 'event';  kind: 'node_added' | 'node_removed'; nodeId: string }

type ClientMessage =
  | { type: 'input';  gesture: GestureEvent }
  | { type: 'pose';   head: Matrix4x4; leftHand?: HandData; rightHand?: HandData }
  | { type: 'resize'; width: number; height: number }

interface SpatialNode {
  id:      string
  label:   string
  icon:    string
  layer:   number
  pos:     [number, number, number]
  visible: number  // 0.0 - 1.0
  color:   string
}
```

---

## 11. Criterios de Aceptación

### Rendimiento
- [ ] 60fps estables en pantalla plana (GPU media: RX 580 / GTX 1060)
- [ ] 72fps en Meta Quest 3 (modo standalone via ALVR)
- [ ] Latencia input→render < 20ms en modo desktop
- [ ] Latencia end-to-end (compositor → Quest) < 30ms en WiFi 5GHz
- [ ] Uso de memoria < 200MB base sin aplicaciones

### Funcionalidad
- [ ] Navegación entre capas Z con scroll/teclado
- [ ] Al menos 20 ventanas simultáneas sin degradación
- [ ] Persistencia de posición de ventanas entre sesiones
- [ ] Hot-reload de configuración sin reinicio
- [ ] Fallback gracioso a modo 2D si no hay hardware XR

### Compatibilidad
- [ ] Cualquier app Wayland corre sin modificaciones
- [ ] XWayland para apps X11 legacy
- [ ] Funciona en: Arch Linux, Ubuntu 24.04, Fedora 40
- [ ] Meta Quest 3 via ALVR (WiFi 6 recomendado)
- [ ] Android 10+ con ARCore para WebXR

### Calidad de código
- [ ] Cobertura de tests unitarios ≥ 70% en libspatial-core
- [ ] Sin memory leaks (valgrind clean en suite de tests)
- [ ] Documentación Doxygen en todas las APIs públicas
- [ ] Lint: clang-tidy sin warnings en nivel strict

---

## Referencias

- [The Wayland Book](https://wayland-book.com) — protocolo Wayland
- [wlroots wiki](https://gitlab.freedesktop.org/wlroots/wlroots/-/wikis) — base del compositor
- [OpenXR spec 1.0](https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html)
- [Monado](https://monado.freedesktop.org) — runtime OpenXR Linux
- [xrdesktop](https://gitlab.freedesktop.org/xrdesktop/xrdesktop) — referencia: GNOME en VR
- [Simula](https://github.com/SimulaVR/Simula) — referencia: compositor Wayland en VR
- [ALVR](https://github.com/alvr-org/ALVR) — streaming PC→Quest
- [Hyprland](https://github.com/hyprwm/Hyprland) — referencia de compositor moderno C++
- [learnopengl.com](https://learnopengl.com) — OpenGL para el renderizado

---

*Generado por SPATIAL OS Design Session — Claude Sonnet 4.6*  
*Última actualización: Febrero 2026*
