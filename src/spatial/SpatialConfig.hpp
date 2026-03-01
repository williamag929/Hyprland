#pragma once

#include <string>
#include <map>

// ══════════════════════════════════════════════════════════════════════════════
// SPATIAL OS: Configuration
// ══════════════════════════════════════════════════════════════════════════════
// Parser and runtime configuration manager for the spatial{} hyprlang section
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
    /// spatial {
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

        // ── Load / parse ──────────────────────────────────────────────
        /// @brief Load configuration from file.
        /// @deprecated [SPATIAL] TASK-SH-401 — spatial{} is now registered with hyprlang via
        ///             CConfigManager::registerConfigVar() and consumed via onSpatialConfigReload()
        ///             on every config load/reload.  This hand-rolled file reader is no longer
        ///             called from production code and will be removed in a future cleanup pass.
        /// @param configPath  Absolute path to hyprland.conf
        /// @return true if loaded and validated successfully, false on I/O or parse error
        [[deprecated("Use CConfigManager::onSpatialConfigReload() — TASK-SH-401")]]
        bool loadFromFile(const std::string& configPath);

        /// @brief Reload configuration from the previously loaded path (hot-reload)
        /// @return true if reloaded successfully; false if never loaded or file error
        bool reload();

        /// @brief Check whether the configuration was loaded successfully at least once
        /// @return true after a successful loadFromFile(), false otherwise
        [[nodiscard]] bool isLoaded() const;

        // ── Accessors ───────────────────────────────────────────────
        /// @brief Get the number of discrete Z layers
        /// @return Layer count in [1, 16]
        [[nodiscard]] int getZLayerCount() const;

        /// @brief Get world-unit distance between adjacent layers
        /// @return Layer step in (0, ∞]
        [[nodiscard]] float getZLayerStep() const;

        /// @brief Get animation spring stiffness
        /// @return Stiffness constant ≥ 1.0
        [[nodiscard]] float getZAnimationStiffness() const;

        /// @brief Get animation spring damping
        /// @return Damping constant ≥ 0.0
        [[nodiscard]] float getZAnimationDamping() const;

        /// @brief Get vertical field-of-view in degrees
        /// @return FOV in (0.0, 180.0)
        [[nodiscard]] float getZFOVDegrees() const;

        /// @brief Get the perspective near plane distance
        /// @return Near plane in (0.0, far)
        [[nodiscard]] float getZNearPlane() const;

        /// @brief Get the perspective far plane distance
        /// @return Far plane > near
        [[nodiscard]] float getZFarPlane() const;

        /// @brief Get whether the spatial Z-layer system is enabled
        /// @return true if spatial navigation is active (default), false if disabled via config
        [[nodiscard]] bool isEnabled() const;

        /// @brief Get whether AR passthrough compositing is enabled
        /// @return true if ar_passthrough = 1 in the spatial config section
        /// @note Defaults to false — AR hardware is not required to run the compositor
        [[nodiscard]] bool isArPassthroughEnabled() const;

        /// @brief Get the global AR blend weight
        /// @return Value in [0.0, 1.0]: 0=full camera, 1=full compositor (default 1.0)
        [[nodiscard]] float getArAlpha() const;

        // ── Debug ─────────────────────────────────────────────────────
        /// @brief Print current configuration (debug)
        void debugPrint() const;

      private:
        // ── Parsed values (safe defaults used when $spatial section is absent) ──
        int                                m_iZLayerCount         = 4;        ///< Number of discrete Z layers (clamped [1,16])
        float                              m_fZLayerStep          = 800.0f;   ///< World-unit distance between layers (> 0)
        float                              m_fZAnimationStiffness = 200.0f;   ///< Spring stiffness (≥ 1.0)
        float                              m_fZAnimationDamping   = 20.0f;    ///< Spring damping (≥ 0.0)
        float                              m_fZFOVDegrees         = 60.0f;    ///< Vertical FOV in degrees (0 < fov < 180)
        float                              m_fZNearPlane          = 0.1f;     ///< Perspective near plane (> 0, < far)
        float                              m_fZFarPlane           = 10000.0f; ///< Perspective far plane (> near)

        bool                               m_bEnabled              = true;  ///< false when config sets "enabled = false" in spatial block
        bool                               m_bArPassthroughEnabled = false; ///< [SPATIAL] TASK-SH-301: true when ar_passthrough = 1
        float                              m_fArAlpha              = 1.0f;  ///< [SPATIAL] TASK-SH-301: global AR blend [0.0–1.0]
        bool                               m_bLoaded               = false; ///< Set true after the first successful loadFromFile()
        std::string                        m_sConfigPath;                   ///< Path passed to the last loadFromFile() call
        std::map<std::string, std::string> m_mValues;                       ///< Raw key→value pairs from the $spatial block

        /// @brief Core line-by-line parser for the extracted $spatial block
        /// @param configContent  Full text of hyprland.conf
        /// @return true if section parsed without fatal errors; false on structural failure
        bool parseConfigSection(const std::string& configContent);

        /// @brief Clamp all parsed values to safe ranges after parsing
        /// @note  Called unconditionally at the end of parseConfigSection().
        ///        Emits a std::cerr warning for every value that was out of range.
        void validateAndClamp();
    };

} // namespace Spatial
