#pragma once

#include <functional>

// ══════════════════════════════════════════════════════════════════════════════
// SPATIAL OS: Input Handler for Z-Space Navigation
// ══════════════════════════════════════════════════════════════════════════════
// Gestiona inputs (scroll, keybinds) para navegación en eje Z
// ══════════════════════════════════════════════════════════════════════════════

namespace Spatial {

/// @class SpatialInputHandler
/// @brief Maneja inputs espaciales (scroll, teclas)
///
/// Intercepta eventos de input y los traduce a navegación Z:
/// - Scroll sin modificador → navegación discreta entre capas
/// - Teclas spatial_next_layer / spatial_prev_layer
/// - Drag derecho → rotación libre (futuro)
/// - Zoom continuo en Z (futuro)
class SpatialInputHandler {
public:
    SpatialInputHandler();
    ~SpatialInputHandler() = default;

    // Tipos de callback para eventos Z
    using LayerChangeCallback = std::function<void(int newLayer, int oldLayer)>;
    using CameraZChangeCallback = std::function<void(float newZ, float oldZ)>;

    // ── Callbacks ─────────────────────────────────────────────────
    /// @brief Registra callback para cambios de capa
    void setLayerChangeCallback(LayerChangeCallback cb);

    /// @brief Registra callback para cambios de cámara Z
    void setCameraZChangeCallback(CameraZChangeCallback cb);

    // ── Input Processing ──────────────────────────────────────────
    /// @brief Procesa evento de scroll (rueda) en eje Y/X
    /// @param scrollY Scroll vertical (positivo = arriba/atrás)
    /// @param hasModifier true si se presiona un modificador (Ctrl, Alt, etc)
    void processScrollEvent(float scrollY, bool hasModifier);

    /// @brief Procesa evento de keybind "spatial_next_layer"
    void processNextLayerKeybind();

    /// @brief Procesa evento de keybind "spatial_prev_layer"
    void processPrevLayerKeybind();

    // ── Configuración ─────────────────────────────────────────────
    /// @brief Establece sensibilidad del scroll para navegación Z
    /// @param sensitivity Factor de escala (default ~1.0)
    void setScrollSensitivity(float sensitivity);

    /// @brief Obtiene sensibilidad actual
    float getScrollSensitivity() const;

    // ── Debug ─────────────────────────────────────────────────────
    /// @brief Imprime configuración de inputs (debug)
    void debugPrint() const;

private:
    float m_fScrollSensitivity = 1.0f;
    int   m_iScrollAccumulator = 0;

    LayerChangeCallback m_fnLayerChangeCallback;
    CameraZChangeCallback m_fnCameraZChangeCallback;
};

}  // namespace Spatial
