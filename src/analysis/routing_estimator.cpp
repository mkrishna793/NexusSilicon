#include "mfe/analysis/routing_estimator.hpp"

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace mfe::analysis {
namespace {

double capacity_for_cell(const core::MeshCell& cell, const pdk::Technology& technology) {
    double capacity{};
    for (const auto& layer : technology.routing_layers()) {
        const auto pitch = static_cast<double>(layer.pitch);
        switch (layer.preferred_direction) {
            case pdk::PreferredDirection::horizontal: capacity += static_cast<double>(cell.bounds.height()) / pitch; break;
            case pdk::PreferredDirection::vertical: capacity += static_cast<double>(cell.bounds.width()) / pitch; break;
            case pdk::PreferredDirection::bidirectional:
                capacity += (static_cast<double>(cell.bounds.width()) + static_cast<double>(cell.bounds.height())) / (2.0 * pitch);
                break;
        }
    }
    return capacity;
}

std::vector<std::size_t> cells_on_segment(const core::UniformMesh& mesh, core::Point first, core::Point second) {
    std::vector<std::size_t> result;
    if (first.x != second.x && first.y != second.y) return result;
    const auto minimum_x = std::min(first.x, second.x);
    const auto maximum_x = std::max(first.x, second.x);
    const auto minimum_y = std::min(first.y, second.y);
    const auto maximum_y = std::max(first.y, second.y);
    for (std::size_t row = 0; row < mesh.rows(); ++row) {
        for (std::size_t column = 0; column < mesh.columns(); ++column) {
            const auto& bounds = mesh.cell(column, row).bounds;
            const bool horizontal = first.y == second.y && first.y >= bounds.bottom && first.y < bounds.top &&
                                    maximum_x > bounds.left && minimum_x < bounds.right;
            const bool vertical = first.x == second.x && first.x >= bounds.left && first.x < bounds.right &&
                                  maximum_y > bounds.bottom && minimum_y < bounds.top;
            if (horizontal || vertical) result.push_back(row * mesh.columns() + column);
        }
    }
    return result;
}

std::string instance_name(const std::string& connection) {
    return connection.substr(0, connection.find('/'));
}

}  // namespace

void assign_routing_capacity(core::UniformMesh& mesh, const pdk::Technology& technology) {
    if (!technology.validates()) throw std::invalid_argument("routing capacity requires a valid technology");
    for (std::size_t row = 0; row < mesh.rows(); ++row)
        for (std::size_t column = 0; column < mesh.columns(); ++column)
            mesh.cell(column, row).capacity = capacity_for_cell(mesh.cell(column, row), technology);
}

RoutingEstimate estimate_routing_demand(core::UniformMesh& mesh, const io::Design& design, RoutingEstimatePolicy policy) {
    if (!design.validates() || policy.demand_per_connection < 0.0)
        throw std::invalid_argument("routing demand requires a valid design and non-negative demand");
    std::unordered_map<std::string, core::Point> locations;
    for (const auto& component : design.components) locations.emplace(component.name, component.location);
    RoutingEstimate estimate;
    for (const auto& net : design.nets) {
        std::vector<core::Point> endpoints;
        for (const auto& connection : net.connections) {
            const auto found = locations.find(instance_name(connection));
            if (found != locations.end()) endpoints.push_back(found->second);
        }
        if (endpoints.size() < 2) { estimate.skipped_connections += net.connections.size(); continue; }
        const auto source = endpoints.front();
        for (std::size_t endpoint = 1; endpoint < endpoints.size(); ++endpoint) {
            const core::Point bend{endpoints[endpoint].x, source.y};
            std::unordered_set<std::size_t> crossed;
            for (const auto cell : cells_on_segment(mesh, source, bend)) crossed.insert(cell);
            for (const auto cell : cells_on_segment(mesh, bend, endpoints[endpoint])) crossed.insert(cell);
            if (crossed.empty()) { ++estimate.skipped_connections; continue; }
            for (const auto cell : crossed) {
                mesh.cell(cell % mesh.columns(), cell / mesh.columns()).demand += policy.demand_per_connection;
                estimate.total_cell_demand += policy.demand_per_connection;
            }
            ++estimate.routed_connections;
        }
    }
    return estimate;
}

}  // namespace mfe::analysis
