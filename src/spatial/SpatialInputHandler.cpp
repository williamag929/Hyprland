#include "SpatialInputHandler.hpp"
#include <iostream>
#include <cmath>

namespace Spatial {

// ══════════════════════════════════════════════════════════════════════════════
// SPATIAL OS: Input Handler Implementation
// ══════════════════════════════════════════════════════════════════════════════

SpatialInputHandler::SpatialInputHandler()
    : m_fScrollSensitivity(1.0f), m_iScrollAccumulator(0) {
}

void SpatialInputHandler::setLayerChangeCallback(LayerChangeCallback cb) {
    m_fnLayerChangeCallback = cb;
}

void SpatialInputHandler::setCameraZChangeCallback(CameraZChangeCallback cb) {
    m_fnCameraZChangeCallback = cb;
}

void SpatialInputHandler::processScrollEvent(float scrollY, bool hasModifier) {
    // Si hay modificador, ignorar (el scroll normal lo maneja Hyprland)
    if (hasModifier) {
        return;
    }

    // Acumular scroll discreto hasta alcanzar un "paso" de capa
    m_iScrollAccumulator += (int)(scrollY * 120.0f * m_fScrollSensitivity);

    const int SCROLL_PER_LAYER = 120;  // líneas de scroll por cambio de capa

    while (m_iScrollAccumulator >= SCROLL_PER_LAYER) {
        processNextLayerKeybind();
        m_iScrollAccumulator -= SCROLL_PER_LAYER;
    }

    while (m_iScrollAccumulator <= -SCROLL_PER_LAYER) {
        processPrevLayerKeybind();
        m_iScrollAccumulator += SCROLL_PER_LAYER;
    }
}

void SpatialInputHandler::processNextLayerKeybind() {
    // Callback será manejado por el compositor (llamará a ZSpaceManager::nextLayer())
    if (m_fnLayerChangeCallback) {
        // El compositor pasará las capas actual y nueva
        // Por ahora solo notificamos
        std::cout << "[SpatialInputHandler] Next layer triggered" << std::endl;
    }
}

void SpatialInputHandler::processPrevLayerKeybind() {
    // Callback será manejado por el compositor (llamará a ZSpaceManager::prevLayer())
    if (m_fnLayerChangeCallback) {
        std::cout << "[SpatialInputHandler] Previous layer triggered" << std::endl;
    }
}

void SpatialInputHandler::setScrollSensitivity(float sensitivity) {
    m_fScrollSensitivity = std::max(0.1f, sensitivity);
}

float SpatialInputHandler::getScrollSensitivity() const {
    return m_fScrollSensitivity;
}

void SpatialInputHandler::debugPrint() const {
    std::cout << "[SpatialInputHandler] Scroll sensitivity: " << m_fScrollSensitivity
              << std::endl;
}

}  // namespace Spatial
