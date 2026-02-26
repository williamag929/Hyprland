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
/// @brief Spatial OS configuration manager
///
/// Reads $spatial section from hyprland.conf and extracts parameters for:
/// - Number of Z layers
/// - Distance between layers
/// - Animation parameters (stiffness, damping)
/// - Perspective settings (FOV, near/far planes)
///
/// Example usage in hyprland.conf:
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
    /// @param configPath Path to hyprland.conf file
    /// @return true if loaded successfully, false on error
    bool loadFromFile(const std::string& configPath);

    /// @brief Reload configuration (hot-reload on file change)
    bool reload();

    // ── Acceso a parámetros ──────────────────────────────────────
    /// @brief Obtiene el número de capas Z
    int getZLayerCount() const;

    /// @brief Get distance between layers (units)
    float getZLayerStep() const;

    /// @brief Get animation spring stiffness
    float getZAnimationStiffness() const;

    /// @brief Get animation spring damping
    float getZAnimationDamping() const;

    /// @brief Get FOV in degrees
    float getZFOVDegrees() const;

    /// @brief Get near plane distance
    float getZNearPlane() const;

    /// @brief Get far plane distance
    float getZFarPlane() const;

    // ── Debug ─────────────────────────────────────────────────────
    /// @brief Print current configuration (debug)
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
