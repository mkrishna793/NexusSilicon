#include "mfe/logic/sat_window.hpp"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <map>
#include <set>

namespace mfe {

class DPLLSolver {
public:
    int num_vars = 0;
    std::vector<std::vector<int>> clauses;

    void add_clause(const std::vector<int>& clause) {
        clauses.push_back(clause);
    }

    bool solve(std::vector<int>& assignment) {
        assignment.assign(num_vars + 1, -1);
        return dpll(assignment);
    }

private:
    bool dpll(std::vector<int>& assignment) {
        while (true) {
            bool unit_found = false;
            for (const auto& clause : clauses) {
                int unassigned_count = 0;
                int last_unassigned_lit = 0;
                bool clause_satisfied = false;
                for (int lit : clause) {
                    int var = std::abs(lit);
                    int val = (lit > 0) ? 1 : 0;
                    if (assignment[var] == val) {
                        clause_satisfied = true;
                        break;
                    } else if (assignment[var] == -1) {
                        unassigned_count++;
                        last_unassigned_lit = lit;
                    }
                }
                if (clause_satisfied) continue;
                if (unassigned_count == 0) {
                    return false;
                }
                if (unassigned_count == 1) {
                    int var = std::abs(last_unassigned_lit);
                    assignment[var] = (last_unassigned_lit > 0) ? 1 : 0;
                    unit_found = true;
                    break;
                }
            }
            if (!unit_found) break;
        }

        int next_var = -1;
        for (int i = 1; i <= num_vars; ++i) {
            if (assignment[i] == -1) {
                next_var = i;
                break;
            }
        }
        if (next_var == -1) {
            return true;
        }

        for (int val = 0; val <= 1; ++val) {
            std::vector<int> backup = assignment;
            assignment[next_var] = val;
            if (dpll(assignment)) {
                return true;
            }
            assignment = backup;
        }

        return false;
    }
};

SATWindowRouter::SATWindowRouter(int width, int height)
    : width_(width), height_(height) {}

void SATWindowRouter::add_net(uint32_t net_id, int sx, int sy, int tx, int ty) {
    nets_.push_back({net_id, sx, sy, tx, ty});
}

void SATWindowRouter::add_obstruction(int x, int y) {
    obstructions_.push_back({x, y});
}

bool SATWindowRouter::route(std::vector<DetailedRoute>& routes) {
    routes.clear();
    if (nets_.empty()) return true;

    DPLLSolver solver;
    int num_cells = width_ * height_;
    int num_nets = static_cast<int>(nets_.size());
    solver.num_vars = num_cells * num_nets;

    auto get_var = [&](int x, int y, int net_idx) -> int {
        return 1 + (x + y * width_) + net_idx * num_cells;
    };

    auto is_valid = [&](int x, int y) -> bool {
        return x >= 0 && x < width_ && y >= 0 && y < height_;
    };

    auto get_neighbors = [&](int x, int y) -> std::vector<std::pair<int, int>> {
        std::vector<std::pair<int, int>> nbs;
        int dx[] = {0, 0, -1, 1};
        int dy[] = {-1, 1, 0, 0};
        for (int i = 0; i < 4; ++i) {
            int nx = x + dx[i];
            int ny = y + dy[i];
            if (is_valid(nx, ny)) {
                nbs.push_back({nx, ny});
            }
        }
        return nbs;
    };

    // 1. Overlap Exclusion
    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            for (int n1 = 0; n1 < num_nets; ++n1) {
                for (int n2 = n1 + 1; n2 < num_nets; ++n2) {
                    solver.add_clause({-get_var(x, y, n1), -get_var(x, y, n2)});
                }
            }
        }
    }

    // 2. Obstructions
    for (const auto& obs : obstructions_) {
        for (int n = 0; n < num_nets; ++n) {
            solver.add_clause({-get_var(obs.first, obs.second, n)});
        }
    }

    // 3. Pin Occupancy and Connectivity
    for (int n = 0; n < num_nets; ++n) {
        const auto& net = nets_[n];
        
        // Pin occupancy clauses
        solver.add_clause({get_var(net.sx, net.sy, n)});
        solver.add_clause({get_var(net.tx, net.ty, n)});

        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                int v_var = get_var(x, y, n);
                auto nbs = get_neighbors(x, y);

                bool is_pin = (x == net.sx && y == net.sy) || (x == net.tx && y == net.ty);

                if (is_pin) {
                    // Exactly 1 active neighbor
                    // At least one active:
                    std::vector<int> at_least_one = {-v_var};
                    for (const auto& nb : nbs) {
                        at_least_one.push_back(get_var(nb.first, nb.second, n));
                    }
                    solver.add_clause(at_least_one);

                    // At most one active:
                    for (size_t i = 0; i < nbs.size(); ++i) {
                        for (size_t j = i + 1; j < nbs.size(); ++j) {
                            solver.add_clause({-v_var, -get_var(nbs[i].first, nbs[i].second, n), -get_var(nbs[j].first, nbs[j].second, n)});
                        }
                    }
                } else {
                    // Non-pin: exactly 2 active neighbors if occupied
                    if (nbs.size() < 2) {
                        // Cannot be occupied if it has less than 2 neighbors
                        solver.add_clause({-v_var});
                        continue;
                    }

                    // At least 2 active:
                    // If v is active, we cannot have |nbs|-1 or more inactive neighbors.
                    // Meaning, for any subset of neighbors of size |nbs|-1, they cannot all be false.
                    // This means for any neighbor u, the rest cannot be all false.
                    // Equivalent to: for each neighbor u, if it is the only one active, it is invalid (at least 2 are active).
                    // Let's list combinations of size |nbs|-1:
                    // For example, if neighbors are u1, u2, u3:
                    // Subsets of size 2 must not be both false:
                    // (u1 or u2), (u1 or u3), (u2 or u3).
                    // Generally: for any subset of size |nbs|-1, we add a clause with v_var negated and all variables in the subset.
                    int k = static_cast<int>(nbs.size());
                    // Subsets of size k - 1
                    for (int i = 0; i < k; ++i) {
                        std::vector<int> clause = {-v_var};
                        for (int j = 0; j < k; ++j) {
                            if (i != j) {
                                clause.push_back(get_var(nbs[j].first, nbs[j].second, n));
                            }
                        }
                        solver.add_clause(clause);
                    }

                    // At most 2 active:
                    // For any 3 neighbors, they cannot be all true.
                    for (size_t i = 0; i < nbs.size(); ++i) {
                        for (size_t j = i + 1; j < nbs.size(); ++j) {
                            for (size_t l = j + 1; l < nbs.size(); ++l) {
                                solver.add_clause({
                                    -v_var,
                                    -get_var(nbs[i].first, nbs[i].second, n),
                                    -get_var(nbs[j].first, nbs[j].second, n),
                                    -get_var(nbs[l].first, nbs[l].second, n)
                                });
                            }
                        }
                    }
                }
            }
        }
    }

    std::vector<int> assignment;
    if (!solver.solve(assignment)) {
        return false;
    }

    // Reconstruct paths
    for (int n = 0; n < num_nets; ++n) {
        const auto& net = nets_[n];
        DetailedRoute route;
        route.net_id = net.id;

        std::vector<std::pair<int, int>> path;
        std::set<std::pair<int, int>> visited;

        int cx = net.sx;
        int cy = net.sy;
        path.push_back({cx, cy});
        visited.insert({cx, cy});

        while (cx != net.tx || cy != net.ty) {
            auto nbs = get_neighbors(cx, cy);
            bool found_next = false;
            for (const auto& nb : nbs) {
                if (visited.find(nb) == visited.end()) {
                    int var = get_var(nb.first, nb.second, n);
                    if (assignment[var] == 1) {
                        cx = nb.first;
                        cy = nb.second;
                        path.push_back({cx, cy});
                        visited.insert({cx, cy});
                        found_next = true;
                        break;
                    }
                }
            }
            if (!found_next) {
                // Should not happen for a valid SAT solution
                break;
            }
        }
        route.path = path;
        routes.push_back(route);
    }

    return true;
}

} // namespace mfe
