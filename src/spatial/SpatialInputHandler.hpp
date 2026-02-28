#pragma once

#include <functional>
#include "ZSpaceManager.hpp"  // for Z_LAYERS_COUNT

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

    // ── Layer State Sync ──────────────────────────────────────────────────────
    /// @brief Notify handler of the compositor's current layer index
    /// @param layer  Current active layer from ZSpaceManager (0…Z_LAYERS_COUNT-1)
    /// @note  Call this whenever ZSpaceManager::setActiveLayer() succeeds, so the
    ///        handler's internal state stays in sync and callbacks receive accurate
    ///        oldLayer/newLayer values.
    void setCurrentLayer(int layer);

    /// @brief Get the layer index currently tracked by the handler
    /// @return Layer index in [0, Z_LAYERS_COUNT-1]
    [[nodiscard]] int getCurrentLayer() const;

    // ── Configuration ────────────────────────────────────────────────────────    /// @brief Enable or disable spatial Z-layer navigation entirely
    /// @param enabled  true = spatial active (default); false = all Z inputs pass through
    /// @note  Can be toggled at runtime for hot-reload support
    void setEnabled(bool enabled);

    /// @brief Get whether spatial navigation is currently active
    /// @return true if enabled
    [[nodiscard]] bool isEnabled() const;
    /// @brief Set scroll sensitivity for Z navigation
    /// @param sensitivity Scale factor (default ~1.0)
    void setScrollSensitivity(float sensitivity);

    /// @brief Get current scroll sensitivity
    /// @return Sensitivity scale factor (always ≥ 0.1)
    [[nodiscard]] float getScrollSensitivity() const;

    /// @brief Set the scroll accumulator threshold for one layer step
    /// @param threshold  Integer scroll units required per layer change (default 120)
    void setScrollThreshold(int threshold);

    /// @brief Get the current scroll threshold
    /// @return Threshold in scroll units
    [[nodiscard]] int getScrollThreshold() const;

    // ── Debug ─────────────────────────────────────────────────────
    /// @brief Print input configuration (debug)
    void debugPrint() const;

private:
    float m_fScrollSensitivity  = 1.0f;    ///< Multiplier applied to raw scroll delta
    float m_fScrollAccumulator  = 0.0f;    ///< Fractional accumulator — float to avoid truncation
    int   m_iScrollThreshold    = 120;     ///< Scroll units required to trigger one layer step
    int   m_iCurrentLayer       = 0;       ///< Mirror of ZSpaceManager's active layer index
    bool  m_bEnabled            = true;    ///< When false, all Z navigation inputs are silently ignored

    LayerChangeCallback   m_fnLayerChangeCallback;    ///< Fired on every discrete layer change
    CameraZChangeCallback m_fnCameraZChangeCallback;  ///< Fired on continuous camera Z change
};

}  // namespace Spatial
