#pragma once

#include <functional>

// ══════════════════════════════════════════════════════════════════════════════
// SPATIAL OS: Input Handler for Z-Space Navigation
// ══════════════════════════════════════════════════════════════════════════════
// Gestiona inputs (scroll, keybinds) para navegación en eje Z
// ══════════════════════════════════════════════════════════════════════════════

namespace Spatial {

/// @class SpatialInputHandler
/// @brief Handles spatial inputs (scroll, keys)
///
/// Intercepts input events and translates them to Z navigation:
/// - Scroll without modifier → discrete navigation between layers
/// - spatial_next_layer / spatial_prev_layer keybinds
/// - Right drag → free rotation (future)
/// - Continuous Z zoom (future)
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

    /// @brief Register callback for camera Z changes
    void setCameraZChangeCallback(CameraZChangeCallback cb);

    // ── Input Processing ──────────────────────────────────────────
    /// @brief Process scroll event (wheel) on Y/X axis
    /// @param scrollY Vertical scroll (positive = up/back)
    /// @param hasModifier true if modifier is pressed (Ctrl, Alt, etc)
    void processScrollEvent(float scrollY, bool hasModifier);

    /// @brief Process "spatial_next_layer" keybind event
    void processNextLayerKeybind();

    /// @brief Process "spatial_prev_layer" keybind event
    void processPrevLayerKeybind();

    // ── Configuration ────────────────────────────────────────────────────
    /// @brief Set scroll sensitivity for Z navigation
    /// @param sensitivity Scale factor (default ~1.0)
    void setScrollSensitivity(float sensitivity);

    /// @brief Get current scroll sensitivity
    float getScrollSensitivity() const;

    // ── Debug ─────────────────────────────────────────────────────
    /// @brief Print input configuration (debug)
    void debugPrint() const;

private:
    float m_fScrollSensitivity = 1.0f;
    int   m_iScrollAccumulator = 0;

    LayerChangeCallback m_fnLayerChangeCallback;
    CameraZChangeCallback m_fnCameraZChangeCallback;
};

}  // namespace Spatial
