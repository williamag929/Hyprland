#pragma once

#include <string>
#include <map>

// ══════════════════════════════════════════════════════════════════════════════
// SPATIAL OS: Configuration
// ══════════════════════════════════════════════════════════════════════════════
// Parser y gestor de configuración espacial desde hyprlang (sección $spatial)
// ══════════════════════════════════════════════════════════════════════════════

namespace Spatial {

/// @class SpatialConfig
/// @brief Gestor de configuración Spatial OS
///
/// Lee la sección $spatial del archivo hyprland.conf y extrae parámetros de:
/// - Número de capas Z
/// - Distancia entre capas
/// - Parámetros de animación (stiffness, damping)
/// - Ajustes de perspectiva (FOV, near/far planes)
///
/// Ejemplo de uso en hyprland.conf:
/// ```
/// $spatial {
///     z_layers = 4
///     z_layer_step = 800
///     z_animation_stiffness = 200
///     z_animation_damping = 20
///     z_fov_degrees = 60
/// }
/// ```
class SpatialConfig {
public:
    SpatialConfig();
    ~SpatialConfig() = default;

    // ── Carga y parsing ───────────────────────────────────────────
    /// @brief Carga la configuración desde archivo
    /// @param configPath Ruta al archivo hyprland.conf
    /// @return true si se cargó correctamente, false en error
    bool loadFromFile(const std::string& configPath);

    /// @brief Recarga la configuración (hot-reload on file change)
    bool reload();

    // ── Acceso a parámetros ──────────────────────────────────────
    /// @brief Obtiene el número de capas Z
    int getZLayerCount() const;

    /// @brief Obtiene la distancia entre capas (unidades)
    float getZLayerStep() const;

    /// @brief Obtiene rigidez de animación spring
    float getZAnimationStiffness() const;

    /// @brief Obtiene amortiguación de animación spring
    float getZAnimationDamping() const;

    /// @brief Obtiene FOV en grados
    float getZFOVDegrees() const;

    /// @brief Obtiene distancia near plane
    float getZNearPlane() const;

    /// @brief Obtiene distancia far plane
    float getZFarPlane() const;

    // ── Debug ─────────────────────────────────────────────────────
    /// @brief Imprime configuración actual (debug)
    void debugPrint() const;

private:
    int   m_iZLayerCount           = 4;
    float m_fZLayerStep            = 800.0f;
    float m_fZAnimationStiffness   = 200.0f;
    float m_fZAnimationDamping     = 20.0f;
    float m_fZFOVDegrees           = 60.0f;
    float m_fZNearPlane            = 0.1f;
    float m_fZFarPlane             = 10000.0f;

    std::string m_sConfigPath;
    std::map<std::string, std::string> m_mValues;

    bool parseConfigSection(const std::string& configContent);
};

}  // namespace Spatial
