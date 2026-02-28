#include "SpatialConfig.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace Spatial {

// ══════════════════════════════════════════════════════════════════════════════
// SPATIAL OS: Configuration Implementation
// ══════════════════════════════════════════════════════════════════════════════

SpatialConfig::SpatialConfig() {
}

bool SpatialConfig::loadFromFile(const std::string& configPath) {
    m_sConfigPath = configPath;

    std::ifstream file(configPath);
    if (!file.is_open()) {
        std::cerr << "[SpatialConfig] Failed to open: " << configPath << "\n";
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    if (!parseConfigSection(content))
        return false;

    m_bLoaded = true;
    return true;
}

bool SpatialConfig::reload() {
    if (m_sConfigPath.empty())
        return false;
    // Reset loaded flag so callers can detect a failed reload
    m_bLoaded = false;
    return loadFromFile(m_sConfigPath);
}

bool SpatialConfig::isLoaded() const {
    return m_bLoaded;
}

bool SpatialConfig::parseConfigSection(const std::string& configContent) {
    // Find $spatial { ... } section
    size_t spatialPos = configContent.find("$spatial");
    if (spatialPos == std::string::npos) {
        // Missing section is not an error — safe defaults are already set
        std::cerr << "[SpatialConfig] $spatial section not found, using defaults\n";
        validateAndClamp();
        return true;
    }

    size_t openBrace = configContent.find('{', spatialPos);
    if (openBrace == std::string::npos) {
        std::cerr << "[SpatialConfig] $spatial section found but has no opening '{'\n";
        return false;
    }

    int braceCount = 1;
    size_t pos = openBrace + 1;

    while (pos < configContent.length() && braceCount > 0) {
        if (configContent[pos] == '{') {
            braceCount++;
        } else if (configContent[pos] == '}') {
            braceCount--;
        }
        pos++;
    }

    std::string sectionContent = configContent.substr(openBrace + 1, pos - openBrace - 2);

    // Simple parser: search for "key = value"
    std::istringstream iss(sectionContent);
    std::string line;

    while (std::getline(iss, line)) {
        // Remove whitespace and comments
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos)
            line = line.substr(0, commentPos);

        size_t equalPos = line.find('=');
        if (equalPos == std::string::npos)
            continue;

        std::string key   = line.substr(0, equalPos);
        std::string value = line.substr(equalPos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);

        // Remove trailing semicolon if present
        if (!value.empty() && value.back() == ';')
            value.pop_back();

        m_mValues[key] = value;

        // Parse specific values — each wrapped to prevent compositor crash on typos
        try {
            if (key == "z_layers") {
                m_iZLayerCount = std::stoi(value);
            } else if (key == "z_layer_step") {
                m_fZLayerStep = std::stof(value);
            } else if (key == "z_animation_stiffness") {
                m_fZAnimationStiffness = std::stof(value);
            } else if (key == "z_animation_damping") {
                m_fZAnimationDamping = std::stof(value);
            } else if (key == "z_fov_degrees") {
                m_fZFOVDegrees = std::stof(value);
            } else if (key == "z_near_plane") {
                m_fZNearPlane = std::stof(value);
            } else if (key == "z_far_plane") {
                m_fZFarPlane = std::stof(value);
            }
        } catch (const std::invalid_argument& e) {
            std::cerr << "[SpatialConfig] Invalid value for '" << key
                      << "' (\"" << value << "\") — keeping default. (" << e.what() << ")\n";
        } catch (const std::out_of_range& e) {
            std::cerr << "[SpatialConfig] Value out of numeric range for '" << key
                      << "' (\"" << value << "\") — keeping default. (" << e.what() << ")\n";
        }
    }

    validateAndClamp();
    return true;
}

void SpatialConfig::validateAndClamp() {
    // z_layers: must be [1, 16] — 0 or negative causes UB on LAYER_Z_POSITIONS indexing
    if (m_iZLayerCount < 1 || m_iZLayerCount > 16) {
        std::cerr << "[SpatialConfig] z_layers=" << m_iZLayerCount
                  << " is out of valid range [1,16], clamping\n";
        m_iZLayerCount = std::clamp(m_iZLayerCount, 1, 16);
    }

    // z_layer_step: must be > 0 — zero collapses all layers to the same Z
    if (m_fZLayerStep <= 0.0f) {
        std::cerr << "[SpatialConfig] z_layer_step=" << m_fZLayerStep
                  << " must be > 0, resetting to default 800\n";
        m_fZLayerStep = 800.0f;
    }

    // z_animation_stiffness: must be >= 1 to avoid motionless spring
    if (m_fZAnimationStiffness < 1.0f) {
        std::cerr << "[SpatialConfig] z_animation_stiffness=" << m_fZAnimationStiffness
                  << " must be >= 1.0, clamping\n";
        m_fZAnimationStiffness = 1.0f;
    }

    // z_animation_damping: must be >= 0
    if (m_fZAnimationDamping < 0.0f) {
        std::cerr << "[SpatialConfig] z_animation_damping=" << m_fZAnimationDamping
                  << " must be >= 0.0, clamping\n";
        m_fZAnimationDamping = 0.0f;
    }

    // z_fov_degrees: glm::perspective is undefined at 0 or >= 180
    if (m_fZFOVDegrees <= 0.0f || m_fZFOVDegrees >= 180.0f) {
        std::cerr << "[SpatialConfig] z_fov_degrees=" << m_fZFOVDegrees
                  << " must be in (0, 180), resetting to default 60\n";
        m_fZFOVDegrees = 60.0f;
    }

    // z_near_plane: must be > 0 — glm::perspective divides by (far - near); near=0 → NaN
    if (m_fZNearPlane <= 0.0f) {
        std::cerr << "[SpatialConfig] z_near_plane=" << m_fZNearPlane
                  << " must be > 0, resetting to default 0.1\n";
        m_fZNearPlane = 0.1f;
    }

    // z_far_plane: must be strictly greater than near
    if (m_fZFarPlane <= m_fZNearPlane) {
        std::cerr << "[SpatialConfig] z_far_plane=" << m_fZFarPlane
                  << " must be > z_near_plane (" << m_fZNearPlane
                  << "), resetting to default 10000\n";
        m_fZFarPlane = 10000.0f;
    }
}

int SpatialConfig::getZLayerCount() const {
    return m_iZLayerCount;
}

float SpatialConfig::getZLayerStep() const {
    return m_fZLayerStep;
}

float SpatialConfig::getZAnimationStiffness() const {
    return m_fZAnimationStiffness;
}

float SpatialConfig::getZAnimationDamping() const {
    return m_fZAnimationDamping;
}

float SpatialConfig::getZFOVDegrees() const {
    return m_fZFOVDegrees;
}

float SpatialConfig::getZNearPlane() const {
    return m_fZNearPlane;
}

float SpatialConfig::getZFarPlane() const {
    return m_fZFarPlane;
}

void SpatialConfig::debugPrint() const {
    std::cout << "[SpatialConfig] loaded=" << (m_bLoaded ? "yes" : "no") << "\n"
              << "  z_layers:              " << m_iZLayerCount << "\n"
              << "  z_layer_step:          " << m_fZLayerStep << "\n"
              << "  z_animation_stiffness: " << m_fZAnimationStiffness << "\n"
              << "  z_animation_damping:   " << m_fZAnimationDamping << "\n"
              << "  z_fov_degrees:         " << m_fZFOVDegrees << "\n"
              << "  z_near_plane:          " << m_fZNearPlane << "\n"
              << "  z_far_plane:           " << m_fZFarPlane << "\n";
    std::cout.flush();
}

}  // namespace Spatial
