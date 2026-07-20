#pragma once

#include "mfe/flow/manifest.hpp"
#include "mfe/transport/power_diagram.hpp"
#include "mfe/io/netlist_importer.hpp"
#include "mfe/io/routed_def_writer.hpp"
#include <filesystem>
#include <vector>

namespace mfe::io {

class OasisWriter {
public:
    OasisWriter() = default;

    // Writes design database (placed cells and routed wire tracks) to binary OASIS format
    bool write_oasis(const std::filesystem::path& path,
                     const std::string& design_name,
                     const std::vector<Cell>& cells,
                     const std::vector<SourceLink>& links,
                     const std::vector<RoutedNet>& routed_nets,
                     double dbu_per_micron);
};

} // namespace mfe::io
