#pragma once

#include <vector>
#include <cstdint>
#include <utility>

namespace mfe {

struct DetailedRoute {
    uint32_t net_id;
    std::vector<std::pair<int, int>> path;
};

class SATWindowRouter {
public:
    SATWindowRouter(int width, int height);

    void add_net(uint32_t net_id, int sx, int sy, int tx, int ty);
    void add_obstruction(int x, int y);

    /**
     * @brief Solves the detailed routing problem within this window.
     * @param routes Output vector of detailed routes for each net.
     * @return True if a feasible routing exists, false otherwise.
     */
    bool route(std::vector<DetailedRoute>& routes);

private:
    int width_;
    int height_;
    struct NetInfo {
        uint32_t id;
        int sx, sy;
        int tx, ty;
    };
    std::vector<NetInfo> nets_;
    std::vector<std::pair<int, int>> obstructions_;
};

} // namespace mfe
