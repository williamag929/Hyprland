# Cambios Espaciales [SPATIAL] — Fork Hyprland v0.45.x

> Registro de modificaciones para Spatial OS integración  
> Última actualización: Febrero 26, 2026

---

## ✅ Cambios Completados

### Fase 1: Estructura de Datos Z

**[SPATIAL] src/Compositor.hpp**
- Agregado `#include "spatial/ZSpaceManager.hpp"` al inicio (post Color Management)
- Agregado `inline UP<Spatial::ZSpaceManager> g_pZSpaceManager;` al final

**[SPATIAL] src/Compositor.cpp**
- Agregado inicialización de `g_pZSpaceManager` en `initManagers(STAGE_PRIORITY)`
- Log: "Creating the ZSpaceManager!"

**[SPATIAL] src/desktop/view/Window.hpp**
- Agregado struct `SSpatialProps` con campos:
  - `float fZPosition` (posición Z actual)
  - `float fZTarget` (posición Z objetivo)
  - `float fZVelocity` (velocidad de animación)
  - `int iZLayer` (capa discreta 0-N)
  - `float fDepthNorm` (normalizado 0-1 para shaders)
  - `bool bZPinned` (no afectada por cámara)
  - `bool bZManaged` (app controla su Z)
- Field declarado como `m_sSpatialProps` en clase CWindow

**[SPATIAL] src/managers/input/InputManager.cpp**
- Interceptación de scroll wheel en `onMouseWheel()`
- Scroll vertical positivo → `nextLayer()`, negativo → `prevLayer()`
- Consume el evento (return sin procesar al resto)

---

## 🔄 Cambios en Progreso

### Fase 2: Renderizado con Perspectiva

**PENDIENTE: src/render/Renderer.cpp**
- [ ] Implementar `getSpatialProjection()` con glm::perspective
- [ ] Implementar `getSpatialView()` con glm::lookAt
- [ ] Integrar depth sorting en renderAllWindows()
- [ ] Pasar uniforms u_zDepth y u_blurRadius a shaders

### Fase 3: Integración de Shaders

**PENDIENTE: src/render/Shader.cpp**
- [ ] Cargar depth_spatial.frag, depth_dof.frag, passthrough_ar.frag
- [ ] Compilar con glslangValidator validatio

**PENDIENTE: Renderer.cpp**
- [ ] Vincular shaders de profundidad al pipeline

---

## 📋 Cambios Pendientes

### Fase 4: Inicialización en renderMonitor()

**PENDIENTE: src/render/Renderer.cpp::renderMonitor()**
- [ ] Primera llamada: `g_pZSpaceManager->init(w, h)`
- [ ] Update por frame: `g_pZSpaceManager->update(deltaTime)`

### Fase 5: Asignación de Ventanas a Capas

**PENDIENTE: src/desktop/view/Window.cpp o Compositor.cpp**
- [ ] Llamar `g_pZSpaceManager->assignWindowToLayer(window, layer)` en mapWindow()
- [ ] Sincronizar Z en animaciones existentes

### Fase 6: Validación de Configuración

**PENDIENTE: src/spatial/SpatialConfig.cpp integration**
- [ ] Cargar configuración desde hyprlang
- [ ] Aplicar parámetros de animación

---

## 🔧 Archivos Nuevos Creados

```
✅ src/spatial/ZSpaceManager.hpp/cpp     — Gestor de capas Z
✅ src/spatial/SpatialConfig.hpp/cpp     — Parser de configuración
✅ src/spatial/SpatialInputHandler.hpp/cpp — Manejo de inputs
✅ src/render/shaders/depth_spatial.frag — Shader principal
✅ src/render/shaders/depth_dof.frag     — Depth of field
✅ src/render/shaders/passthrough_ar.frag — AR passthrough (placeholder)
✅ .github/workflows/spatial-build.yml   — CI/CD pipeline
```

---

## 📝 Próximos Pasos

1. **Implementar renderizado 3D** — matrices de perspectiva en Renderer.cpp
2. **Integrar shaders** — cargar y compilar depth shaders
3. **Sincronizar animaciones** — usar spring physics en update()
4. **Configuración** — parser de sección $spatial en hyprland.conf
5. **Testing** — suite de tests unitarios con Google Test

---

## 🚧 Notas de Diseño

- **Thread-safety:** ZSpaceManager usa mutex interno para acceso desde InputManager
- **Compatibilidad upstream:** Todos los cambios en Hyprland se prefijan con [SPATIAL]
- **Mínima invasión:** SSpatialProps es struct aislado, no modifica lógica existente de Window
- **Scroll interception:** Solo captura scroll sin modificadores (permite scroll normal con Shift, Ctrl, etc.)

---

## ⚠️ Riesgos y Mitigación

| Riesgo | Mitigación |
|--------|-----------|
| Conflictos con upstream | Usar [SPATIAL] prefix, mínimos cambios en archivos core |
| Performance degradation | Spring physics optimizado, input throttling |
| Memory leaks | valgrind clean en CI/CD, RAII patterns |
| Z-ordering issues | Depth sorting en Renderer, SSpatialProps vinculado a Window |

---

*Documento: SPATIAL_CHANGES.md*  
*Fork: spatial-hypr (Hyprland v0.45.x)*  
*Estado: En desarrollo — Fase 1-2*
