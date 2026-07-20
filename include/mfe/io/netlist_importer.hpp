#pragma once

#include "mfe/flow/manifest.hpp"
#include "mfe/transport/power_diagram.hpp"
#include "mfe/transport/ot_placer.hpp"
#include <filesystem>
#include <string>
#include <vector>
#include <unordered_map>

namespace mfe::io {

struct SourceLink {
    std::string rtl_symbol;
    std::string synthesized_cell;
    uint32_t physical_instance_id;
    double width_um{0.0};
    double height_um{0.0};
    std::unordered_map<std::string, std::pair<double, double>> pin_offsets_um;
    std::unordered_map<std::string, std::string> pin_layers;
};

class NetlistImporter {
public:
    NetlistImporter() = default;

    // Reads the mapped Verilog netlist and PDK libraries, populating cells and nets.
    // Creates floorplan constraints based on the design boundary size.
    bool import_design(const flow::FlowProject& project,
                       const flow::PdkManifest& pdk,
                       std::vector<Cell>& cells,
                       std::vector<mfe::Net>& nets);

    // Returns the resolved logical-to-physical source links
    [[nodiscard]] const std::vector<SourceLink>& get_source_links() const noexcept { return source_links_; }
    [[nodiscard]] const std::vector<std::string>& get_net_names() const noexcept { return net_names_; }
    [[nodiscard]] const std::vector<std::vector<std::pair<uint32_t, std::string>>>& get_net_connections() const noexcept {
        return net_connections_;
    }

private:
    std::vector<SourceLink> source_links_;
    std::vector<std::string> net_names_;
    std::vector<std::vector<std::pair<uint32_t, std::string>>> net_connections_;
    std::unordered_map<std::string, std::pair<double, double>> cell_dimensions_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::pair<double, double>>> cell_pin_centers_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> cell_pin_layers_;

    // Reads cell dimensions (width, height) from the cell LEFs
    bool parse_cell_libraries(const flow::PdkManifest& pdk);
    
    // Parses the mapped Verilog file line-by-line
    bool parse_verilog(const std::filesystem::path& path,
                       std::vector<Cell>& cells,
                       std::vector<Net>& nets);
};

} // namespace mfe::io
