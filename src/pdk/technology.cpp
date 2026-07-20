#include "mfe/pdk/technology.hpp"

#include <algorithm>

namespace mfe::pdk {

Technology::Technology(std::string name, core::DbUnit database_units_per_micron)
    : name_(std::move(name)), database_units_per_micron_(database_units_per_micron) {}

void Technology::add_routing_layer(RoutingLayer layer) { routing_layers_.push_back(std::move(layer)); }
void Technology::add_via_rule(ViaRule rule) { via_rules_.push_back(std::move(rule)); }

const RoutingLayer* Technology::find_layer(const std::string& name) const noexcept {
    const auto found = std::find_if(routing_layers_.begin(), routing_layers_.end(),
                                   [&name](const RoutingLayer& layer) { return layer.name == name; });
    return found == routing_layers_.end() ? nullptr : &*found;
}

bool Technology::validates() const noexcept {
    if (name_.empty() || database_units_per_micron_ <= 0 || routing_layers_.empty()) return false;
    for (const auto& layer : routing_layers_) {
        if (layer.name.empty() || layer.level < 1 || layer.pitch <= 0 || layer.minimum_width <= 0 ||
            layer.minimum_spacing < 0 || layer.minimum_width + layer.minimum_spacing > layer.pitch) return false;
    }
    return true;
}

}  // namespace mfe::pdk
