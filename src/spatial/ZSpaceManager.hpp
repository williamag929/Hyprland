#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <memory>

// Forward declarations
namespace Desktop::View {
    class CWindow;
}

// ══════════════════════════════════════════════════════════════════════════════
// SPATIAL OS: Z-Space Management
// ══════════════════════════════════════════════════════════════════════════════
// Gestiona la posición Z de ventanas, capas de profundidad, y animaciones.
// Thread-safe mediante mutex interno.
// ══════════════════════════════════════════════════════════════════════════════

namespace Spatial {

// Constantes del sistema Z
constexpr int   Z_LAYERS_COUNT      = 4;
constexpr float Z_LAYER_STEP        = 800.0f;   // unidades entre capas
constexpr float Z_NEAR              = 0.1f;
constexpr float Z_FAR               = 10000.0f;
constexpr float Z_FOV_DEGREES       = 60.0f;
constexpr float Z_ANIM_STIFFNESS    = 200.0f;  // rigidez del spring
constexpr float Z_ANIM_DAMPING      = 20.0f;   // amortiguación

// Posición Z por capa
constexpr float LAYER_Z_POSITIONS[Z_LAYERS_COUNT] = {
    0.0f,      // Capa 0: Foreground — app activa
   -800.0f,    // Capa 1: Near — apps recientes
  -1600.0f,    // Capa 2: Mid — segundo plano
  -2800.0f,    // Capa 3: Far — sistema / config
};

// Visibilidad por distancia de capa
constexpr float LAYER_OPACITY[Z_LAYERS_COUNT] = {
    1.00f,   // Capa 0: opacidad total
    0.75f,   // Capa 1: ligero fade
    0.40f,   // Capa 2: claramente al fondo
    0.15f,   // Capa 3: casi invisible
};

constexpr float LAYER_BLUR_RADIUS[Z_LAYERS_COUNT] = {
    0.0f,    // Capa 0: sin blur
    1.5f,    // Capa 1: blur sutil
    5.0f,    // Capa 2: blur evidente
   12.0f,    // Capa 3: blur fuerte
};

// ──────────────────────────────────────────────────────────────────────────────
/// @class ZSpaceManager
/// @brief Gestor centralizado de navegación espacial en eje Z
///
/// Responsabilidades:
/// - Asignar ventanas a capas Z y coordenadas continuas
/// - Actualizar posición de cámara según capa activa
/// - Animar transiciones de posición con spring physics
/// - Mantener orden de renderizado (depth sorting)
/// - Validar límites y restricciones de profundidad
///
/// Thread-safety: Usa mutex interno — métodos son const-thread-safe
// ──────────────────────────────────────────────────────────────────────────────
class ZSpaceManager {
public:
    ZSpaceManager();
    ~ZSpaceManager();

    // ── Inicialización ────────────────────────────────────────────
    /// @brief Inicializa el gestor Z con parámetros de pantalla
    /// @param screenWidth  Ancho de pantalla en píxeles
    /// @param screenHeight Alto de pantalla en píxeles
    void init(int screenWidth, int screenHeight);

    // ── Navegación de capas ───────────────────────────────────────
    /// @brief Avanza a la siguiente capa (más lejana)
    /// @return true si cambió de capa, false si ya estaba en la última
    bool nextLayer();

    /// @brief Retrocede a la capa anterior (más cercana)
    /// @return true si cambió de capa, false si ya estaba en la primera
    bool prevLayer();

    /// @brief Cambia a una capa específica
    /// @param layer Índice de capa (0 a Z_LAYERS_COUNT-1)
    /// @return true si el cambio fue exitoso
    bool setActiveLayer(int layer);

    /// @brief Obtiene la capa activa actual
    int getActiveLayer() const;

    // ── Posicionamiento Z ─────────────────────────────────────────
    /// @brief Obtiene la posición Z de la cámara
    /// @return Coordenada Z en unidades de mundo
    float getCameraZ() const;

    /// @brief Asigna una ventana a una capa específica
    /// @brief Asigna una ventana a una capa Z específica
    /// @param window Ventana (CWindow*)
    /// @param layer  Índice de capa (0 a Z_LAYERS_COUNT-1)
    void assignWindowToLayer(CWindow* window, int layer);

    /// @brief Obtiene la capa de una ventana
    /// @param window Ventana (CWindow*)
    /// @return Índice de capa, o -1 si no está registrada
    int getWindowLayer(CWindow* window) const;

    /// @brief Obtiene la posición Z de una ventana
    /// @param window Ventana (CWindow*)
    /// @return Coordenada Z en unidades de mundo
    float getWindowZ(CWindow* window) const;

    /// @brief Asigna posición Z continua a una ventana (override de capa)
    /// @param window Ventana (CWindow*)
    /// @param z      Posición Z en unidades de mundo
    void setWindowZPosition(CWindow* window, float z);

    // ── Animación ─────────────────────────────────────────────────
    /// @brief Actualiza animaciones Z (llamar cada frame)
    /// @param deltaTime Tiempo transcurrido desde el frame anterior (segundos)
    void update(float deltaTime);

    /// @brief Obtiene la posición Z objetivo de una ventana
    /// @param window Ventana (CWindow*)
    /// @return Z objetivo (para debug/visualización)
    float getWindowZTarget(CWindow* window) const;

    /// @brief Obtiene la velocidad Z actual de una ventana
    /// @param window Ventana (CWindow*)
    /// @return Velocidad en unidades/segundo
    float getWindowZVelocity(CWindow* window) const;

    // ── Matrices de proyección ────────────────────────────────────
    /// @brief Calcula matriz de proyección perspectiva
    /// @return Matriz 4x4 de perspectiva OpenGL
    glm::mat4 getSpatialProjection() const;

    /// @brief Calcula matriz de vista (cámara)
    /// @return Matriz 4x4 de vista
    glm::mat4 getSpatialView() const;

    /// @brief Obtiene matriz ModelTransform para una ventana
    /// @param window Ventana (CWindow*)
    /// @return Matriz 4x4 que incluye traslación XYZ
    glm::mat4 getWindowTransform(CWindow* window) const;

    // ── Propiedades derivadas ─────────────────────────────────────
    /// @brief Calcula opacidad normalizada (0-1) para una ventana
    /// @param window Ventana (CWindow*)
    /// @return Opacidad en [0, 1]
    float getWindowOpacity(CWindow* window) const;

    /// @brief Calcula radio de blur (px) para una ventana
    /// @param window Puntero a la ventana
    /// @return Radio de blur Gaussiano
    float getWindowBlurRadius(CWindow* window) const;

    // ── Debug ─────────────────────────────────────────────────────
    /// @brief Imprime estado actual (debug)
    void debugPrint() const;

private:
    // Estructura interna para ventanas
    struct WindowZ {
        void*  pWindow      = nullptr;
        float  fZPosition   = 0.0f;
        float  fZTarget     = 0.0f;
        float  fZVelocity   = 0.0f;
        int    iZLayer      = 0;
        bool   bZPinned     = false;  // no afectada por cámara
    };

    std::vector<WindowZ> m_vWindowsZ;
    int                  m_iActiveLayer    = 0;
    float                m_fCameraZ        = 0.0f;
    float                m_fCameraZTarget  = 0.0f;
    int                  m_iScreenW        = 0;
    int                  m_iScreenH        = 0;

    // Thread safety (será inicialized en init())
    void* m_pMutex = nullptr;

    WindowZ* findWindow(void* window);
    const WindowZ* findWindow(void* window) const;

    void updateSpringAnimation(WindowZ& wz, float dt);
};

}  // namespace Spatial
