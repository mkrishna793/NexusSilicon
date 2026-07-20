#pragma once

#include "mfe/transport/power_diagram.hpp"
#include "mfe/transport/ot_placer.hpp"
#include <string>
#include <vector>
#include <unordered_map>

namespace mfe {

struct BookshelfLayout {
    std::vector<Cell> cells;
    std::vector<Net> nets;
    // Map from cell name to its assigned Cell ID
    std::unordered_map<std::string, uint32_t> cell_name_to_id;
    // Map from cell ID back to its name
    std::vector<std::string> cell_id_to_name;
    // Track which cell IDs are fixed
    std::vector<bool> is_fixed;
};

// Parses Bookshelf files given paths to .nodes, .nets, and .pl.
// Returns a BookshelfLayout struct containing the parsed Cells and Nets.
BookshelfLayout parse_bookshelf(const std::string& nodes_path,
                                const std::string& nets_path,
                                const std::string& pl_path);

} // namespace mfe
