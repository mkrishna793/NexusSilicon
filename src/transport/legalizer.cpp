#include "mfe/transport/legalizer.hpp"
#include <cmath>
#include <algorithm>
#include <limits>

namespace mfe {

Legalizer::Legalizer(const HalfEdgeMesh& mesh) : mesh_(mesh) {}

int32_t Legalizer::find_nearest_vertex(double x, double y) const {
    double min_d2 = std::numeric_limits<double>::max();
    int32_t best_v = -1;
    for (size_t i = 0; i < mesh_.vertices.size(); ++i) {
        const auto& v = mesh_.vertices[i];
        double dx = v.x - x;
        double dy = v.y - y;
        double d2 = dx * dx + dy * dy;
        if (d2 < min_d2) {
            min_d2 = d2;
            best_v = static_cast<int32_t>(i);
        }
    }
    return best_v;
}

void Legalizer::legalize(std::vector<Cell>& cells, const std::vector<std::pair<double, double>>& site_positions) {
    std::vector<Site> sites;
    sites.reserve(site_positions.size());
    for (const auto& pos : site_positions) {
        Site s;
        s.x = pos.first;
        s.y = pos.second;
        s.occupied = false;
        sites.push_back(s);
    }

    // Sort cells by x coordinate to do standard left-to-right greedy assignment
    std::vector<Cell*> sorted_cells;
    sorted_cells.reserve(cells.size());
    for (auto& c : cells) {
        sorted_cells.push_back(&c);
    }
    std::sort(sorted_cells.begin(), sorted_cells.end(), [](Cell* a, Cell* b) {
        return a->x < b->x;
    });

    for (auto* c : sorted_cells) {
        // Find local metric scale at cell's current position
        int32_t v_idx = find_nearest_vertex(c->x, c->y);
        double scale = 1.0;
        if (v_idx != -1) {
            scale = std::exp(mesh_.vertices[v_idx].conformal_factor);
        }

        double min_cost = std::numeric_limits<double>::max();
        int32_t best_site_idx = -1;

        // Find unoccupied site with minimum metric-weighted distance
        for (size_t i = 0; i < sites.size(); ++i) {
            if (sites[i].occupied) continue;
            
            double dx = c->x - sites[i].x;
            double dy = c->y - sites[i].y;
            double cost = scale * (dx * dx + dy * dy);

            if (cost < min_cost) {
                min_cost = cost;
                best_site_idx = static_cast<int32_t>(i);
            }
        }

        // Snap cell to the best site
        if (best_site_idx != -1) {
            c->x = sites[best_site_idx].x;
            c->y = sites[best_site_idx].y;
            sites[best_site_idx].occupied = true;
        }
    }
}

} // namespace mfe
