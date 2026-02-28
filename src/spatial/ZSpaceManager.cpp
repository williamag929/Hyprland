#include "ZSpaceManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include "../desktop/view/Window.hpp"

// Bring CWindow into scope so it can be used inside namespace Spatial
using Desktop::View::CWindow;

namespace Spatial {

// ══════════════════════════════════════════════════════════════════════════════
// SPATIAL OS: Z-Space Manager Implementation
// ══════════════════════════════════════════════════════════════════════════════

ZSpaceManager::ZSpaceManager()
    : m_iActiveLayer(0), m_fCameraZ(0.0f), m_fCameraZTarget(0.0f),
      m_fCameraZVelocity(0.0f), m_iScreenW(0), m_iScreenH(0) {
}

ZSpaceManager::~ZSpaceManager() {
    // std::mutex is automatically destroyed
}

void ZSpaceManager::init(int screenWidth, int screenHeight) {
    m_iScreenW = screenWidth;
    m_iScreenH = screenHeight;

    // Camera begins at layer 0
    m_fCameraZ = LAYER_Z_POSITIONS[0];
    m_fCameraZTarget = m_fCameraZ;
}

bool ZSpaceManager::nextLayer() {
    if (m_iActiveLayer >= Z_LAYERS_COUNT - 1) {
        return false;
    }
    return setActiveLayer(m_iActiveLayer + 1);
}

bool ZSpaceManager::prevLayer() {
    if (m_iActiveLayer <= 0) {
        return false;
    }
    return setActiveLayer(m_iActiveLayer - 1);
}

bool ZSpaceManager::setActiveLayer(int layer) {
    if (layer < 0 || layer >= Z_LAYERS_COUNT) {
        return false;
    }

    m_iActiveLayer = layer;
    m_fCameraZTarget = LAYER_Z_POSITIONS[layer];
    return true;
}

int ZSpaceManager::getActiveLayer() const {
    return m_iActiveLayer;
}

float ZSpaceManager::getCameraZ() const {
    return m_fCameraZ;
}

void ZSpaceManager::assignWindowToLayer(void* window, int layer) {
    if (layer < 0 || layer >= Z_LAYERS_COUNT || !window) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_oMutex);

    WindowZ* wz = findWindow(window);
    if (!wz) {
        // New window - create entry with initial Z position
        const float initialZ = LAYER_Z_POSITIONS[layer];
        m_vWindowsZ.push_back(WindowZ{window, initialZ, initialZ, 0.0f, layer, false});
    } else {
        // Existing window - update layer
        wz->iZLayer = layer;
        wz->fZTarget = LAYER_Z_POSITIONS[layer];
    }
    
    // [SPATIAL] Immediately update window properties to enable spatial rendering
    CWindow* pWindow = reinterpret_cast<CWindow*>(window);
    pWindow->m_sSpatialProps.bZManaged = true;
    pWindow->m_sSpatialProps.iZLayer = layer;
    pWindow->m_sSpatialProps.fZPosition = LAYER_Z_POSITIONS[layer];
    pWindow->m_sSpatialProps.fZTarget = LAYER_Z_POSITIONS[layer];
}

int ZSpaceManager::getWindowLayer(void* window) const {
    std::lock_guard<std::mutex> lock(m_oMutex);

    const WindowZ* wz = findWindow(window);
    return (wz ? wz->iZLayer : -1);
}

void ZSpaceManager::removeWindow(void* window) {
    if (!window)
        return;

    std::lock_guard<std::mutex> lock(m_oMutex);

    auto it = std::find_if(m_vWindowsZ.begin(), m_vWindowsZ.end(),
                           [window](const WindowZ& wz) { return wz.pWindow == window; });
    if (it != m_vWindowsZ.end())
        m_vWindowsZ.erase(it);
}

float ZSpaceManager::getWindowZ(void* window) const {
    std::lock_guard<std::mutex> lock(m_oMutex);

    const WindowZ* wz = findWindow(window);
    return (wz ? wz->fZPosition : 0.0f);
}

void ZSpaceManager::setWindowZPosition(void* window, float z) {
    std::lock_guard<std::mutex> lock(m_oMutex);

    WindowZ* wz = findWindow(window);
    if (wz) {
        wz->fZTarget = z;
    }
}

void ZSpaceManager::update(float deltaTime) {
    // [SPATIAL] Clamp deltaTime to prevent spring integration instability on lag spikes
    // or first frame. With stiffness=200 and Euler integration, dt > ~0.1s diverges.
    deltaTime = std::clamp(deltaTime, 0.0f, 0.05f);

    std::lock_guard<std::mutex> lock(m_oMutex);

    // Update camera Z with proper spring physics
    {
        float delta = m_fCameraZTarget - m_fCameraZ;
        float accel = (Z_ANIM_STIFFNESS * delta) - (Z_ANIM_DAMPING * m_fCameraZVelocity);
        m_fCameraZVelocity += accel * deltaTime;
        m_fCameraZ += m_fCameraZVelocity * deltaTime;
        if (std::abs(delta) < 0.1f && std::abs(m_fCameraZVelocity) < 0.1f) {
            m_fCameraZ = m_fCameraZTarget;
            m_fCameraZVelocity = 0.0f;
        }
    }

    // Update all windows
    for (auto& wz : m_vWindowsZ) {
        updateSpringAnimation(wz, deltaTime);
        
        // [SPATIAL] Sync internal state back to window properties
        if (wz.pWindow) {
            // Guard: skip if spring produced NaN/Inf (overflow safety)
            if (!std::isfinite(wz.fZPosition)) {
                wz.fZPosition = wz.fZTarget;
                wz.fZVelocity = 0.0f;
            }
            CWindow* pWindow = reinterpret_cast<CWindow*>(wz.pWindow);
            pWindow->m_sSpatialProps.fZPosition = wz.fZPosition;
            pWindow->m_sSpatialProps.fZTarget   = wz.fZTarget;
            pWindow->m_sSpatialProps.fZVelocity = wz.fZVelocity;
            pWindow->m_sSpatialProps.iZLayer    = wz.iZLayer;
            pWindow->m_sSpatialProps.bZPinned   = wz.bZPinned;
            
            // Calculate normalized depth for shaders (0 = far, 1 = near)
            const float zRange = LAYER_Z_POSITIONS[0] - LAYER_Z_POSITIONS[Z_LAYERS_COUNT - 1];  // 2800
            pWindow->m_sSpatialProps.fDepthNorm = (wz.fZPosition - LAYER_Z_POSITIONS[Z_LAYERS_COUNT - 1]) / zRange;
        }
    }
}

float ZSpaceManager::getWindowZTarget(void* window) const {
    std::lock_guard<std::mutex> lock(m_oMutex);

    const WindowZ* wz = findWindow(window);
    return (wz ? wz->fZTarget : 0.0f);
}

float ZSpaceManager::getWindowZVelocity(void* window) const {
    std::lock_guard<std::mutex> lock(m_oMutex);

    const WindowZ* wz = findWindow(window);
    return (wz ? wz->fZVelocity : 0.0f);
}

glm::mat4 ZSpaceManager::getSpatialProjection() const {
    float aspect = (float)m_iScreenW / (float)m_iScreenH;
    return glm::perspective(glm::radians(Z_FOV_DEGREES), aspect, Z_NEAR, Z_FAR);
}

glm::mat4 ZSpaceManager::getSpatialView() const {
    float camZ = m_fCameraZ;
    return glm::lookAt(
        glm::vec3(m_iScreenW / 2.0f, m_iScreenH / 2.0f, camZ + 1200.0f),  // eye
        glm::vec3(m_iScreenW / 2.0f, m_iScreenH / 2.0f, camZ),             // center
        glm::vec3(0.0f, -1.0f, 0.0f)                                       // up (Y inverted)
    );
}

glm::mat4 ZSpaceManager::getWindowTransform(void* window) const {
    std::lock_guard<std::mutex> lock(m_oMutex);

    const WindowZ* wz = findWindow(window);
    glm::mat4 model = glm::mat4(1.0f);

    // TODO: get XY position of window from CWindow
    // For now we only use Z
    if (wz) {
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, wz->fZPosition));
    }

    return model;
}

float ZSpaceManager::getWindowOpacity(void* window) const {
    int layer = getWindowLayer(window);
    if (layer < 0 || layer >= Z_LAYERS_COUNT) {
        return 1.0f;
    }
    return LAYER_OPACITY[layer];
}

float ZSpaceManager::getWindowBlurRadius(void* window) const {
    int layer = getWindowLayer(window);
    if (layer < 0 || layer >= Z_LAYERS_COUNT) {
        return 0.0f;
    }
    return LAYER_BLUR_RADIUS[layer];
}

void ZSpaceManager::debugPrint() const {
    std::cout << "[ZSpaceManager] Active layer: " << m_iActiveLayer
              << ", Camera Z: " << m_fCameraZ << std::endl;
}

ZSpaceManager::WindowZ* ZSpaceManager::findWindow(void* window) {
    auto it = std::find_if(m_vWindowsZ.begin(), m_vWindowsZ.end(),
                           [window](const WindowZ& wz) { return wz.pWindow == window; });
    return (it != m_vWindowsZ.end()) ? &(*it) : nullptr;
}

const ZSpaceManager::WindowZ* ZSpaceManager::findWindow(void* window) const {
    auto it = std::find_if(m_vWindowsZ.begin(), m_vWindowsZ.end(),
                           [window](const WindowZ& wz) { return wz.pWindow == window; });
    return (it != m_vWindowsZ.end()) ? &(*it) : nullptr;
}

void ZSpaceManager::updateSpringAnimation(WindowZ& wz, float dt) {
    // Spring physics: F = -k * (x - target) - d * v
    float delta = wz.fZTarget - wz.fZPosition;
    float accel = (Z_ANIM_STIFFNESS * delta) - (Z_ANIM_DAMPING * wz.fZVelocity);

    wz.fZVelocity += accel * dt;
    wz.fZPosition += wz.fZVelocity * dt;

    // Damping: if very close to target, stop it
    if (std::abs(delta) < 0.1f && std::abs(wz.fZVelocity) < 0.1f) {
        wz.fZPosition = wz.fZTarget;
        wz.fZVelocity = 0.0f;
    }
}

}  // namespace Spatial
