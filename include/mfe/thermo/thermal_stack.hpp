#pragma once

#include <vector>
#include <string>

namespace mfe {

enum class MaterialType {
    Silicon,
    Copper,
    Dielectric
};

/**
 * @brief Get the thermal conductivity of standard materials in W/(m*K)
 */
inline double get_thermal_conductivity(MaterialType material) {
    switch (material) {
        case MaterialType::Silicon:    return 150.0;
        case MaterialType::Copper:     return 400.0;
        case MaterialType::Dielectric: return 1.0;
    }
    return 1.0;
}

struct ThermalLayer {
    std::string name;
    MaterialType material;
    double thickness; // Thickness in arbitrary consistent units (e.g., meters or micrometers)
};

/**
 * @brief Class representing a stack of thermal layers to compute effective thermal conductivities.
 */
class ThermalStack {
public:
    ThermalStack() = default;

    void add_layer(const std::string& name, MaterialType material, double thickness) {
        layers_.push_back({name, material, thickness});
    }

    const std::vector<ThermalLayer>& get_layers() const {
        return layers_;
    }

    /**
     * @brief Computes effective in-plane thermal conductivity: sum(d_i * k_i) / sum(d_i)
     */
    double compute_effective_in_plane_conductivity() const {
        double total_thickness = 0.0;
        double weighted_conductivity = 0.0;
        for (const auto& layer : layers_) {
            double k = get_thermal_conductivity(layer.material);
            weighted_conductivity += layer.thickness * k;
            total_thickness += layer.thickness;
        }
        if (total_thickness <= 0.0) return 1.0;
        return weighted_conductivity / total_thickness;
    }

    /**
     * @brief Computes effective cross-plane thermal conductivity: sum(d_i) / sum(d_i / k_i)
     */
    double compute_effective_cross_plane_conductivity() const {
        double total_thickness = 0.0;
        double sum_thermal_resistance = 0.0;
        for (const auto& layer : layers_) {
            double k = get_thermal_conductivity(layer.material);
            if (k <= 0.0) k = 1.0;
            sum_thermal_resistance += layer.thickness / k;
            total_thickness += layer.thickness;
        }
        if (total_thickness <= 0.0 || sum_thermal_resistance <= 0.0) return 1.0;
        return total_thickness / sum_thermal_resistance;
    }

private:
    std::vector<ThermalLayer> layers_;
};

} // namespace mfe
