#pragma once

#include "mfe/core/half_edge.hpp"
#include <vector>
#include <memory>

namespace mfe {

struct ClockSink {
    uint32_t vertex_id;
    double x;
    double y;
};

struct ClockTreeNode {
    int id;
    double x;
    double y;
    double delay; // Time from this node to its sinks
    int left_child = -1;
    int right_child = -1;
};

class DMECTSBuilder {
public:
    explicit DMECTSBuilder(const HalfEdgeMesh& mesh);

    /**
     * @brief Performs bottom-up merging and top-down embedding of clock sinks.
     * @param sinks List of sink pins.
     * @param nodes Output vector representing the binary clock tree (last node is root).
     * @return True if successful.
     */
    bool build_tree(const std::vector<ClockSink>& sinks, std::vector<ClockTreeNode>& nodes);

private:
    const HalfEdgeMesh& mesh_;

    // Helper to get local conformal factor at a point by finding nearest vertex
    double get_local_lambda(double x, double y) const;
};

} // namespace mfe
