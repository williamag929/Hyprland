#include "ZSpaceManager.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <iostream>
#include <pthread.h>
#include "../desktop/view/Window.hpp"

namespace Spatial {

// ══════════════════════════════════════════════════════════════════════════════
// SPATIAL OS: Z-Space Manager Implementation
// ══════════════════════════════════════════════════════════════════════════════

ZSpaceManager::ZSpaceManager()
    : m_iActiveLayer(0), m_fCameraZ(0.0f), m_fCameraZTarget(0.0f),
      m_iScreenW(0), m_iScreenH(0), m_pMutex(nullptr) {
}

ZSpaceManager::~ZSpaceManager() {
    if (m_pMutex) {
        pthread_mutex_destroy((pthread_mutex_t*)m_pMutex);
        delete (pthread_mutex_t*)m_pMutex;
    }
}

void ZSpaceManager::init(int screenWidth, int screenHeight) {
    m_iScreenW = screenWidth;
    m_iScreenH = screenHeight;

    // Inicializar mutex
    if (!m_pMutex) {
        pthread_mutex_t* mutex = new pthread_mutex_t;
        pthread_mutex_init(mutex, nullptr);
        m_pMutex = (void*)mutex;
    }

    // Cámara comienza en capa 0
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

void ZSpaceManager::assignWindowToLayer(CWindow* window, int layer) {
    if (layer < 0 || layer >= Z_LAYERS_COUNT || !window) {
        return;
    }

    pthread_mutex_lock((pthread_mutex_t*)m_pMutex);

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

    pthread_mutex_unlock((pthread_mutex_t*)m_pMutex);
}

int ZSpaceManager::getWindowLayer(CWindow* window) const {
    pthread_mutex_lock((pthread_mutex_t*)m_pMutex);

    const WindowZ* wz = findWindow(window);
    int layer = (wz ? wz->iZLayer : -1);

    pthread_mutex_unlock((pthread_mutex_t*)m_pMutex);
    return layer;
}

float ZSpaceManager::getWindowZ(CWindow* window) const {
    pthread_mutex_lock((pthread_mutex_t*)m_pMutex);

    const WindowZ* wz = findWindow(window);
    float z = (wz ? wz->fZPosition : 0.0f);

    pthread_mutex_unlock((pthread_mutex_t*)m_pMutex);
    return z;
}

void ZSpaceManager::setWindowZPosition(CWindow* window, float z) {
    pthread_mutex_lock((pthread_mutex_t*)m_pMutex);

    WindowZ* wz = findWindow(window);
    if (wz) {
        wz->fZTarget = z;
    }

    pthread_mutex_unlock((pthread_mutex_t*)m_pMutex);
}

void ZSpaceManager::update(float deltaTime) {
    pthread_mutex_lock((pthread_mutex_t*)m_pMutex);

    // Actualizar cámara
    updateSpringAnimation({nullptr, m_fCameraZ, m_fCameraZTarget, 0.0f, 0, false, true},
                         deltaTime);

    // Actualizar todas las ventanas
    for (auto& wz : m_vWindowsZ) {
        updateSpringAnimation(wz, deltaTime);
        
        // [SPATIAL] Sync internal state back to window properties
        if (wz.pWindow) {
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

    pthread_mutex_unlock((pthread_mutex_t*)m_pMutex);
}

float ZSpaceManager::getWindowZTarget(CWindow* window) const {
    pthread_mutex_lock((pthread_mutex_t*)m_pMutex);

    const WindowZ* wz = findWindow(window);
    float target = (wz ? wz->fZTarget : 0.0f);

    pthread_mutex_unlock((pthread_mutex_t*)m_pMutex);
    return target;
}

float ZSpaceManager::getWindowZVelocity(CWindow* window) const {
    pthread_mutex_lock((pthread_mutex_t*)m_pMutex);

    const WindowZ* wz = findWindow(window);
    float vel = (wz ? wz->fZVelocity : 0.0f);

    pthread_mutex_unlock((pthread_mutex_t*)m_pMutex);
    return vel;
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
        glm::vec3(0.0f, -1.0f, 0.0f)                                       // up (Y invertido)
    );
}

glm::mat4 ZSpaceManager::getWindowTransform(CWindow* window) const {
    pthread_mutex_lock((pthread_mutex_t*)m_pMutex);

    const WindowZ* wz = findWindow(window);
    glm::mat4 model = glm::mat4(1.0f);

    // TODO: obtener posición XY de la ventana desde CWindow
    // Por ahora solo usamos Z
    if (wz) {
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, wz->fZPosition));
    }

    pthread_mutex_unlock((pthread_mutex_t*)m_pMutex);
    return model;
}

float ZSpaceManager::getWindowOpacity(CWindow* window) const {
    int layer = getWindowLayer(window);
    if (layer < 0 || layer >= Z_LAYERS_COUNT) {
        return 1.0f;
    }
    return LAYER_OPACITY[layer];
}

float ZSpaceManager::getWindowBlurRadius(CWindow* window) const {
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

ZSpaceManager::WindowZ* ZSpaceManager::findWindow(CWindow* window) {
    auto it = std::find_if(m_vWindowsZ.begin(), m_vWindowsZ.end(),
                           [window](const WindowZ& wz) { return wz.pWindow == window; });
    return (it != m_vWindowsZ.end()) ? &(*it) : nullptr;
}

const ZSpaceManager::WindowZ* ZSpaceManager::findWindow(PHLWINDOW window) const {
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

    // Damping: si está muy cerca del target, detenerla
    if (std::abs(delta) < 0.1f && std::abs(wz.fZVelocity) < 0.1f) {
        wz.fZPosition = wz.fZTarget;
        wz.fZVelocity = 0.0f;
    }
}

}  // namespace Spatial
