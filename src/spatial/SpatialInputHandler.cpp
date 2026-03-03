#include "SpatialInputHandler.hpp"
#include <algorithm>
#include <iostream>
#include <cmath>

namespace Spatial {

    // ══════════════════════════════════════════════════════════════════════════════
    // SPATIAL OS: Input Handler Implementation
    // ══════════════════════════════════════════════════════════════════════════════

    SpatialInputHandler::SpatialInputHandler() : m_fScrollSensitivity(1.0f), m_fScrollAccumulator(0.0f), m_iScrollThreshold(120), m_iCurrentLayer(0) {}

    void SpatialInputHandler::setLayerChangeCallback(LayerChangeCallback cb) {
        m_fnLayerChangeCallback = cb;
    }

    void SpatialInputHandler::setCameraZChangeCallback(CameraZChangeCallback cb) {
        m_fnCameraZChangeCallback = cb;
    }

    void SpatialInputHandler::processScrollEvent(float scrollY, bool hasModifier) {
        // Spatial disabled — let the event pass through unchanged
        if (!m_bEnabled)
            return;

        // Scroll-based spatial navigation disabled — pass through unchanged
        if (!m_bScrollNavigation)
            return;

        // If modifier is pressed, pass through to Hyprland's normal scroll handler
        if (hasModifier)
            return;

        // Use float accumulator to preserve sub-step scroll (e.g. trackpad deltas of 0.3)
        m_fScrollAccumulator += scrollY * static_cast<float>(m_iScrollThreshold) * m_fScrollSensitivity;

        while (m_fScrollAccumulator >= static_cast<float>(m_iScrollThreshold)) {
            processNextLayerKeybind();
            m_fScrollAccumulator -= static_cast<float>(m_iScrollThreshold);
        }

        while (m_fScrollAccumulator <= -static_cast<float>(m_iScrollThreshold)) {
            processPrevLayerKeybind();
            m_fScrollAccumulator += static_cast<float>(m_iScrollThreshold);
        }
    }

    void SpatialInputHandler::processNextLayerKeybind() {
        if (!m_bEnabled)
            return;

        if (m_iCurrentLayer >= Z_LAYERS_COUNT - 1)
            return; // already at deepest layer — no-op, no spurious callback

        const int oldLayer = m_iCurrentLayer;
        m_iCurrentLayer++;

        if (m_fnLayerChangeCallback)
            m_fnLayerChangeCallback(m_iCurrentLayer, oldLayer);
    }

    void SpatialInputHandler::processPrevLayerKeybind() {
        if (!m_bEnabled)
            return;

        if (m_iCurrentLayer <= 0)
            return; // already at front layer — no-op

        const int oldLayer = m_iCurrentLayer;
        m_iCurrentLayer--;

        if (m_fnLayerChangeCallback)
            m_fnLayerChangeCallback(m_iCurrentLayer, oldLayer);
    }

    bool SpatialInputHandler::toggleScrollNavigation() {
        m_bScrollNavigation = !m_bScrollNavigation;
        return m_bScrollNavigation;
    }

    void SpatialInputHandler::setCurrentLayer(int layer) {
        if (layer >= 0 && layer < Z_LAYERS_COUNT)
            m_iCurrentLayer = layer;
    }

    int SpatialInputHandler::getCurrentLayer() const {
        return m_iCurrentLayer;
    }

    void SpatialInputHandler::setScrollSensitivity(float sensitivity) {
        m_fScrollSensitivity = std::max(0.1f, sensitivity);
    }

    float SpatialInputHandler::getScrollSensitivity() const {
        return m_fScrollSensitivity;
    }

    void SpatialInputHandler::setScrollThreshold(int threshold) {
        m_iScrollThreshold = std::max(1, threshold);
    }

    int SpatialInputHandler::getScrollThreshold() const {
        return m_iScrollThreshold;
    }

    void SpatialInputHandler::setEnabled(bool enabled) {
        m_bEnabled = enabled;
    }

    bool SpatialInputHandler::isEnabled() const {
        return m_bEnabled;
    }

    void SpatialInputHandler::setScrollNavigationEnabled(bool enabled) {
        m_bScrollNavigation = enabled;
    }

    bool SpatialInputHandler::isScrollNavigationEnabled() const {
        return m_bScrollNavigation;
    }

    // ── AR Passthrough ───────────────────────────────────────────────────────────

    void SpatialInputHandler::setArPassthrough(bool enabled) {
        m_bArPassthroughEnabled = enabled;
    }

    bool SpatialInputHandler::isArPassthroughEnabled() const {
        return m_bArPassthroughEnabled;
    }

    void SpatialInputHandler::setArAlpha(float alpha) {
        m_fArAlpha = std::clamp(alpha, 0.0f, 1.0f);
    }

    float SpatialInputHandler::getArAlpha() const {
        return m_fArAlpha;
    }

    void SpatialInputHandler::debugPrint() const {
        std::cout << "[SpatialInputHandler]" << "  current_layer=" << m_iCurrentLayer << "  sensitivity=" << m_fScrollSensitivity << "  threshold=" << m_iScrollThreshold
                  << "  accumulator=" << m_fScrollAccumulator << "  scroll_nav=" << (m_bScrollNavigation ? "on" : "off")
                  << "  layer_cb=" << (m_fnLayerChangeCallback ? "set" : "null")
                  << "  camera_cb=" << (m_fnCameraZChangeCallback ? "set" : "null") << "\n";
        std::cout.flush();
    }

} // namespace Spatial
