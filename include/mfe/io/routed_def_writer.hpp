#pragma once

#include "mfe/transport/power_diagram.hpp"
#include "mfe/io/netlist_importer.hpp"
#include <filesystem>
#include <string>
#include <vector>

namespace mfe::io {

struct WireSegment {
    std::string layer;
    double x1, y1; // in microns
    double x2, y2; // in microns
};

struct RoutedNet {
    std::string name;
    std::vector<std::pair<std::string, std::string>> connections; // pair of (cell_name, pin_name)
    std::vector<WireSegment> segments;
};

// Writes a detailed-routed DEF file with + PLACED coordinates and + ROUTED segments.
bool write_routed_def(const std::filesystem::path& path,
                      const std::string& design_name,
                      const std::vector<Cell>& cells,
                      const std::vector<SourceLink>& links,
                      const std::vector<RoutedNet>& routed_nets,
                      int dbu_per_micron);

} // namespace mfe::io
