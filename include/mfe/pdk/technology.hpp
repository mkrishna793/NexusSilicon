#pragma once

#include "mfe/core/geometry.hpp"

#include <optional>
#include <string>
#include <vector>

namespace mfe::pdk {

enum class PreferredDirection { horizontal, vertical, bidirectional };

struct RoutingLayer {
    std::string name;
    int level{};
    PreferredDirection preferred_direction{PreferredDirection::bidirectional};
    core::DbUnit pitch{};
    core::DbUnit minimum_width{};
    core::DbUnit minimum_spacing{};
    double resistance_ohm_per_um{};
    double capacitance_ff_per_um{};
};

struct ViaRule {
    std::string name;
    std::string lower_layer;
    std::string upper_layer;
    core::DbUnit cut_size{};
    core::DbUnit cut_spacing{};
};

class Technology {
public:
    explicit Technology(std::string name, core::DbUnit database_units_per_micron = 1000);

    void add_routing_layer(RoutingLayer layer);
    void add_via_rule(ViaRule rule);
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] core::DbUnit database_units_per_micron() const noexcept { return database_units_per_micron_; }
    [[nodiscard]] const std::vector<RoutingLayer>& routing_layers() const noexcept { return routing_layers_; }
    [[nodiscard]] const RoutingLayer* find_layer(const std::string& name) const noexcept;
    [[nodiscard]] bool validates() const noexcept;

private:
    std::string name_;
    core::DbUnit database_units_per_micron_;
    std::vector<RoutingLayer> routing_layers_;
    std::vector<ViaRule> via_rules_;
};

}  // namespace mfe::pdk
