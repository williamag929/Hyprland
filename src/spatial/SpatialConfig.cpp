#include "SpatialConfig.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
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
        std::cerr << "[SpatialConfig] No se pudo abrir: " << configPath << std::endl;
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    return parseConfigSection(content);
}

bool SpatialConfig::reload() {
    if (m_sConfigPath.empty()) {
        return false;
    }
    return loadFromFile(m_sConfigPath);
}

bool SpatialConfig::parseConfigSection(const std::string& configContent) {
    // Buscar sección $spatial { ... }
    size_t spatialPos = configContent.find("$spatial");
    if (spatialPos == std::string::npos) {
        // Si no existe la sección, usar valores por defecto
        std::cout << "[SpatialConfig] Sección $spatial no encontrada, usando defaults"
                  << std::endl;
        return true;
    }

    size_t openBrace = configContent.find('{', spatialPos);
    if (openBrace == std::string::npos) {
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

    // Parser simple: buscar "key = value"
    std::istringstream iss(sectionContent);
    std::string line;

    while (std::getline(iss, line)) {
        // Quitar whitespace y comentarios
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            line = line.substr(0, commentPos);
        }

        size_t equalPos = line.find('=');
        if (equalPos == std::string::npos) {
            continue;
        }

        std::string key = line.substr(0, equalPos);
        std::string value = line.substr(equalPos + 1);

        // Trim whitespace
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t\n\r") + 1);

        // Quitar punto y coma al final si existe
        if (!value.empty() && value.back() == ';') {
            value.pop_back();
        }

        m_mValues[key] = value;

        // Parsear valores específicos
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
    }

    return true;
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
    std::cout << "[SpatialConfig]\n"
              << "  Z Layers: " << m_iZLayerCount << "\n"
              << "  Z Layer Step: " << m_fZLayerStep << "\n"
              << "  Animation Stiffness: " << m_fZAnimationStiffness << "\n"
              << "  Animation Damping: " << m_fZAnimationDamping << "\n"
              << "  FOV: " << m_fZFOVDegrees << "°\n"
              << "  Near Plane: " << m_fZNearPlane << "\n"
              << "  Far Plane: " << m_fZFarPlane << std::endl;
}

}  // namespace Spatial
