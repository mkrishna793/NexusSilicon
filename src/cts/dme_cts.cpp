#include "mfe/cts/dme_cts.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>

namespace mfe {

DMECTSBuilder::DMECTSBuilder(const HalfEdgeMesh& mesh) : mesh_(mesh) {}

double DMECTSBuilder::get_local_lambda(double x, double y) const {
    double min_d2 = std::numeric_limits<double>::max();
    double best_lambda = 0.0;
    for (const auto& v : mesh_.vertices) {
        double dx = v.x - x;
        double dy = v.y - y;
        double d2 = dx * dx + dy * dy;
        if (d2 < min_d2) {
            min_d2 = d2;
            best_lambda = v.conformal_factor;
        }
    }
    return best_lambda;
}

bool DMECTSBuilder::build_tree(const std::vector<ClockSink>& sinks, std::vector<ClockTreeNode>& nodes) {
    nodes.clear();
    if (sinks.empty()) return true;

    // 1. Initialize leaf nodes from sinks
    for (size_t i = 0; i < sinks.size(); ++i) {
        ClockTreeNode node;
        node.id = static_cast<int>(nodes.size());
        node.x = sinks[i].x;
        node.y = sinks[i].y;
        node.delay = 0.0; // Sinks start with 0 delay from themselves
        nodes.push_back(node);
    }

    std::vector<int> active_nodes;
    for (size_t i = 0; i < sinks.size(); ++i) {
        active_nodes.push_back(static_cast<int>(i));
    }

    // 2. Hierarchical Pairing (Bottom-Up)
    while (active_nodes.size() > 1) {
        double min_dist = std::numeric_limits<double>::max();
        int best_i = -1;
        int best_j = -1;

        // Find the closest pair of active nodes using metric-scaled distance
        for (size_t i = 0; i < active_nodes.size(); ++i) {
            int node_i = active_nodes[i];
            const auto& ni = nodes[node_i];
            double lambda_i = get_local_lambda(ni.x, ni.y);

            for (size_t j = i + 1; j < active_nodes.size(); ++j) {
                int node_j = active_nodes[j];
                const auto& nj = nodes[node_j];
                double lambda_j = get_local_lambda(nj.x, nj.y);

                double dx = ni.x - nj.x;
                double dy = ni.y - nj.y;
                double d_euc = std::sqrt(dx * dx + dy * dy);

                // Metric scaled distance
                double d_g = std::exp((lambda_i + lambda_j) / 2.0) * d_euc;

                if (d_g < min_dist) {
                    min_dist = d_g;
                    best_i = static_cast<int>(i);
                    best_j = static_cast<int>(j);
                }
            }
        }

        if (best_i == -1 || best_j == -1) {
            return false;
        }

        // Retrieve node IDs
        int id_A = active_nodes[best_i];
        int id_B = active_nodes[best_j];
        const auto& node_A = nodes[id_A];
        const auto& node_B = nodes[id_B];

        double lambda_A = get_local_lambda(node_A.x, node_A.y);
        double lambda_B = get_local_lambda(node_B.x, node_B.y);
        double dx = node_A.x - node_B.x;
        double dy = node_A.y - node_B.y;
        double d_euc = std::sqrt(dx * dx + dy * dy);
        double L = std::exp((lambda_A + lambda_B) / 2.0) * d_euc;

        // Balance skew: delay_A + dist(M, A) = delay_B + dist(M, B)
        // Since dist(M, A) + dist(M, B) = L:
        // delay_A + dist(M, A) = delay_B + L - dist(M, A) -> dist(M, A) = (L + delay_B - delay_A) / 2
        double d_MA = (L + node_B.delay - node_A.delay) / 2.0;
        double f_A = 0.5;
        if (L > 1e-9) {
            f_A = d_MA / L;
            f_A = std::clamp(f_A, 0.0, 1.0);
        }

        // Create Parent Node
        ClockTreeNode parent;
        parent.id = static_cast<int>(nodes.size());
        parent.x = (1.0 - f_A) * node_A.x + f_A * node_B.x;
        parent.y = (1.0 - f_A) * node_A.y + f_A * node_B.y;
        parent.delay = node_A.delay + f_A * L; // Or node_B.delay + (1-f_A)*L
        parent.left_child = id_A;
        parent.right_child = id_B;

        nodes.push_back(parent);

        // Update active nodes list
        // Remove best_j first to preserve index of best_i if j > i
        if (best_j > best_i) {
            active_nodes.erase(active_nodes.begin() + best_j);
            active_nodes.erase(active_nodes.begin() + best_i);
        } else {
            active_nodes.erase(active_nodes.begin() + best_i);
            active_nodes.erase(active_nodes.begin() + best_j);
        }
        active_nodes.push_back(parent.id);
    }

    return true;
}

} // namespace mfe
