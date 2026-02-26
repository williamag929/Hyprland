#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <memory>

// ══════════════════════════════════════════════════════════════════════════════
// SPATIAL OS: Z-Space Management
// ══════════════════════════════════════════════════════════════════════════════
// Manages Z position of windows, depth layers, and animations.
// Thread-safe via internal mutex.
// ══════════════════════════════════════════════════════════════════════════════

namespace Spatial {

// Z-System Constants
constexpr int   Z_LAYERS_COUNT      = 4;
constexpr float Z_LAYER_STEP        = 800.0f;   // units between layers
constexpr float Z_NEAR              = 0.1f;
constexpr float Z_FAR               = 10000.0f;
constexpr float Z_FOV_DEGREES       = 60.0f;
constexpr float Z_ANIM_STIFFNESS    = 200.0f;  // spring stiffness
constexpr float Z_ANIM_DAMPING      = 20.0f;   // damping

// Z position per layer
constexpr float LAYER_Z_POSITIONS[Z_LAYERS_COUNT] = {
    0.0f,      // Layer 0: Foreground — active app
   -800.0f,    // Layer 1: Near — recent apps
  -1600.0f,    // Layer 2: Mid — background
  -2800.0f,    // Layer 3: Far — system / config
};

// Layer opacity by depth distance
constexpr float LAYER_OPACITY[Z_LAYERS_COUNT] = {
    1.00f,   // Layer 0: full opacity
    0.75f,   // Layer 1: slight fade
    0.40f,   // Layer 2: clearly in background
    0.15f,   // Layer 3: nearly invisible
};

constexpr float LAYER_BLUR_RADIUS[Z_LAYERS_COUNT] = {
    0.0f,    // Layer 0: no blur
    1.5f,    // Layer 1: subtle blur
    5.0f,    // Layer 2: evident blur
   12.0f,    // Layer 3: strong blur
};

// ──────────────────────────────────────────────────────────────────────────────
/// @class ZSpaceManager
/// @brief Centralized manager for spatial navigation along Z axis
///
/// Responsibilities:
/// - Assign windows to Z layers and continuous coordinates
/// - Update camera position based on active layer
/// - Animate position transitions with spring physics
/// - Maintain render order (depth sorting)
/// - Validate depth limits and constraints
///
/// Thread-safety: Uses internal mutex — methods are const-thread-safe
// ──────────────────────────────────────────────────────────────────────────────
class ZSpaceManager {
public:
    ZSpaceManager();
    ~ZSpaceManager();

    // ── Initialization ────────────────────────────────────────────
    /// @brief Initialize Z manager with screen parameters
    /// @param screenWidth  Ancho de pantalla en píxeles
    /// @param screenHeight Alto de pantalla en píxeles
    void init(int screenWidth, int screenHeight);

    // ── Layer Navigation ───────────────────────────────────────
    /// @brief Advance to next layer (further away)
    /// @return true si cambió de capa, false si ya estaba en la última
    bool nextLayer();

    /// @brief Return to previous layer (closer)
    /// @return true si cambió de capa, false si ya estaba en la primera
    bool prevLayer();

    /// @brief Change to specific layer
    /// @param layer Índice de capa (0 a Z_LAYERS_COUNT-1)
    /// @return true si el cambio fue exitoso
    bool setActiveLayer(int layer);

    /// @brief Get current active layer
    int getActiveLayer() const;

    // ── Z Positioning ─────────────────────────────────────────
    /// @brief Get Z position of camera
    /// @return Coordenada Z en unidades de mundo
    float getCameraZ() const;

    /// @brief Assign window to Z layer
    /// @param window Pointer to CWindow (void* to avoid circular includes)
    /// @param layer  Layer index (0 to Z_LAYERS_COUNT-1)
    void assignWindowToLayer(void* window, int layer);

    /// @brief Get layer of window
    /// @param window Pointer to CWindow
    /// @return Layer index, or -1 if not registered
    int getWindowLayer(void* window) const;

    /// @brief Get Z position of window
    /// @param window Pointer to CWindow
    /// @return Z coordinate in world units
    float getWindowZ(void* window) const;

    /// @brief Assign continuous Z position to window (layer override)
    /// @param window Pointer to CWindow
    /// @param z      Z position in world units
    void setWindowZPosition(void* window, float z);

    // ── Animation ─────────────────────────────────────────────────
    /// @brief Update Z animations (call each frame)
    /// @param deltaTime Tiempo transcurrido desde el frame anterior (segundos)
    void update(float deltaTime);

    /// @brief Get target Z position of window
    /// @param window Pointer to CWindow
    /// @return Target Z (for debug/visualization)
    float getWindowZTarget(void* window) const;

    /// @brief Get current Z velocity of window
    /// @param window Pointer to CWindow
    /// @return Velocity in units/second
    float getWindowZVelocity(void* window) const;

    // ── Projection Matrices ────────────────────────────────────
    /// @brief Calculate perspective projection matrix
    /// @return Matriz 4x4 de perspectiva OpenGL
    glm::mat4 getSpatialProjection() const;

    /// @brief Calculate view matrix (camera)
    /// @return Matriz 4x4 de vista
    glm::mat4 getSpatialView() const;

    /// @brief Get ModelTransform matrix for window
    /// @param window Ventana (CWindow*)
    /// @return Matriz 4x4 que incluye traslación XYZ
    glm::mat4 getWindowTransform(void* window) const;

    // ── Derived Properties ─────────────────────────────────────
    /// @brief Calculate normalized opacity (0-1) for window
    /// @param window Puntero a ventana CWindow
    /// @return Opacidad en [0, 1]
    float getWindowOpacity(void* window) const;

    /// @brief Calculate blur radius (px) for window
    /// @param window Pointer to CWindow
    /// @return Radio de blur Gaussiano
    float getWindowBlurRadius(void* window) const;

    // ── Debug ─────────────────────────────────────────────────────
    /// @brief Print current state (debug)
    void debugPrint() const;

private:
    // Internal per-window Z state
    struct WindowZ {
        void*  pWindow      = nullptr;
        float  fZPosition   = 0.0f;
        float  fZTarget     = 0.0f;
        float  fZVelocity   = 0.0f;
        int    iZLayer      = 0;
        bool   bZPinned     = false;  // not affected by camera
    };

    std::vector<WindowZ> m_vWindowsZ;
    int                  m_iActiveLayer     = 0;
    float                m_fCameraZ         = 0.0f;
    float                m_fCameraZTarget   = 0.0f;
    float                m_fCameraZVelocity = 0.0f;  // spring velocity for camera
    int                  m_iScreenW         = 0;
    int                  m_iScreenH         = 0;

    // Thread safety (initialized in init())
    void* m_pMutex = nullptr;

    WindowZ* findWindow(void* window);
    const WindowZ* findWindow(void* window) const;

    void updateSpringAnimation(WindowZ& wz, float dt);
};

}  // namespace Spatial
