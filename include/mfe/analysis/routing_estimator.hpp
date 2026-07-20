#pragma once

#include "mfe/core/mesh.hpp"
#include "mfe/io/lef_def.hpp"
#include "mfe/pdk/technology.hpp"

#include <cstddef>

namespace mfe::analysis {

struct RoutingEstimatePolicy {
    // Demand added to every mesh cell crossed by a routed connection.
    double demand_per_connection{1.0};
};

struct RoutingEstimate {
    std::size_t routed_connections{};
    std::size_t skipped_connections{};
    double total_cell_demand{};
};

// Computes each cell's routing supply from layer pitch and preferred direction.
// Existing demand is preserved; existing capacity is replaced.
void assign_routing_capacity(core::UniformMesh& mesh, const pdk::Technology& technology);

// Routes each DEF net as a deterministic Manhattan star from its first placed
// component. Each distinct cell crossed by a connection receives one demand unit.
[[nodiscard]] RoutingEstimate estimate_routing_demand(core::UniformMesh& mesh,
                                                       const io::Design& design,
                                                       RoutingEstimatePolicy policy = {});

}  // namespace mfe::analysis
