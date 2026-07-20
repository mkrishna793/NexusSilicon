#include "mfe/io/netlist_importer.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <regex>

namespace mfe::io {

namespace {
} // namespace

bool NetlistImporter::import_design(const flow::FlowProject& project,
                                     const flow::PdkManifest& pdk,
                                     std::vector<Cell>& cells,
                                     std::vector<mfe::Net>& nets) {
    std::cout << "[Netlist Importer] Parsing cell libraries..." << std::endl;
    if (!parse_cell_libraries(pdk)) {
        std::cerr << "Error: Failed to parse cell LEF libraries." << std::endl;
        return false;
    }

    // Resolve SDC / project constraints paths
    std::filesystem::path netlist_path = project.outputs / "frontend" / "mapped_netlist.v";
    if (!std::filesystem::exists(netlist_path)) {
        // Fallback to project constraints or mock netlist path if running directly in test
        netlist_path = "mapped_netlist.v";
        if (!std::filesystem::exists(netlist_path)) {
            std::cerr << "Error: Mapped netlist not found at: " << netlist_path.string() << std::endl;
            return false;
        }
    }

    std::cout << "[Netlist Importer] Parsing mapped netlist..." << std::endl;
    return parse_verilog(netlist_path, cells, nets);
}

bool NetlistImporter::parse_cell_libraries(const flow::PdkManifest& pdk) {
    cell_dimensions_.clear();
    cell_pin_centers_.clear();
    cell_pin_layers_.clear();
    for (const auto& lef_path : pdk.cell_lefs) {
        std::ifstream file(lef_path);
        if (!file.is_open()) continue;

        std::string line;
        std::string current_macro;
        std::string current_pin;
        std::string current_layer;
        bool in_port = false;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string token;
            if (!(ss >> token)) continue;

            if (token == "MACRO") {
                ss >> current_macro;
                current_pin.clear();
                current_layer.clear();
                in_port = false;
            } else if (token == "SIZE" && !current_macro.empty()) {
                double w = 0.0, h = 0.0;
                std::string by;
                if (ss >> w >> by >> h) {
                    cell_dimensions_[current_macro] = {w, h};
                }
            } else if (token == "PIN" && !current_macro.empty()) {
                ss >> current_pin;
                current_layer.clear();
                in_port = false;
            } else if (token == "PORT" && !current_pin.empty()) {
                current_layer.clear();
                in_port = true;
            } else if (token == "LAYER" && in_port && !current_pin.empty()) {
                ss >> current_layer;
            } else if (token == "RECT" && in_port && !current_macro.empty() && !current_pin.empty()) {
                double x1 = 0.0, y1 = 0.0, x2 = 0.0, y2 = 0.0;
                if (ss >> x1 >> y1 >> x2 >> y2 && !cell_pin_centers_[current_macro].contains(current_pin)) {
                    cell_pin_centers_[current_macro][current_pin] = {(x1 + x2) / 2.0, (y1 + y2) / 2.0};
                    cell_pin_layers_[current_macro][current_pin] = current_layer;
                }
            } else if (token == "END" && !current_pin.empty()) {
                std::string end_name;
                ss >> end_name;
                if (end_name == current_pin) {
                    current_pin.clear();
                    in_port = false;
                }
            } else if (token == "END" && !current_macro.empty()) {
                std::string end_name;
                ss >> end_name;
                if (end_name == current_macro) {
                    current_macro.clear();
                }
            }
        }
    }
    return !cell_dimensions_.empty();
}

bool NetlistImporter::parse_verilog(const std::filesystem::path& path,
                                   std::vector<Cell>& cells,
                                   std::vector<Net>& nets) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    source_links_.clear();
    net_names_.clear();
    net_connections_.clear();
    cells.clear();
    nets.clear();

    std::unordered_map<std::string, std::vector<std::pair<uint32_t, std::string>>> net_to_connections;
    // Yosys writes mapped cells across several lines.  Parse complete Verilog
    // statements, then accept only instances whose macro exists in the PDK LEF.
    std::regex pin_regex(R"(\.([^\s()]+)\s*\(\s*([^\s()]+)\s*\))");
    std::string line;
    std::string statement;
    while (std::getline(file, line)) {
        const auto comment = line.find("//");
        if (comment != std::string::npos) line.erase(comment);
        statement += line;
        statement += '\n';
        if (line.find(';') == std::string::npos) continue;
        const auto last = statement.find_last_not_of(" \t\r\n");
        const std::string candidate = last == std::string::npos ? std::string{} : statement.substr(0, last + 1);
        const auto open_paren = candidate.find('(');
        const auto close_paren = candidate.rfind(')');
        if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren) {
            std::istringstream header(candidate.substr(0, open_paren));
            std::string macro_type;
            std::string inst_name;
            header >> macro_type >> inst_name;
            const std::string pin_mappings = candidate.substr(open_paren + 1, close_paren - open_paren - 1);

            auto dim_it = cell_dimensions_.find(macro_type);
            // Ignore declarations, behavioural constructs, and unmapped cells.
            if (dim_it == cell_dimensions_.end()) {
                statement.clear();
                continue;
            }
            const double width = dim_it->second.first;
            const double height = dim_it->second.second;

            uint32_t cell_id = static_cast<uint32_t>(cells.size());
            Cell cell;
            cell.id = cell_id;
            cell.mass = width * height;
            cell.x = 0.0;
            cell.y = 0.0;
            cell.weight = 0.0;
            
            cells.push_back(cell);

            // Record source links
            SourceLink link;
            link.rtl_symbol = inst_name;
            link.synthesized_cell = macro_type;
            link.physical_instance_id = cell_id;
            link.width_um = width;
            link.height_um = height;
            if (const auto pins = cell_pin_centers_.find(macro_type); pins != cell_pin_centers_.end()) {
                link.pin_offsets_um = pins->second;
            }
            if (const auto pin_layers = cell_pin_layers_.find(macro_type); pin_layers != cell_pin_layers_.end()) {
                link.pin_layers = pin_layers->second;
            }
            source_links_.push_back(link);

            // Parse net pin connections
            auto pins_begin = std::sregex_iterator(pin_mappings.begin(), pin_mappings.end(), pin_regex);
            auto pins_end = std::sregex_iterator();
            for (std::sregex_iterator i = pins_begin; i != pins_end; ++i) {
                std::smatch pin_match = *i;
                const std::string pin_name = pin_match[1].str();
                const std::string net_name = pin_match[2].str();
                if (net_name.find('\'') == std::string::npos) {
                    net_to_connections[net_name].push_back({cell_id, pin_name});
                }
            }
        }
        statement.clear();
    }

    // Convert map to Nets vector
    uint32_t net_id_counter = 0;
    std::vector<std::string> ordered_names;
    ordered_names.reserve(net_to_connections.size());
    for (const auto& [net_name, connections] : net_to_connections) {
        if (connections.size() >= 2) ordered_names.push_back(net_name);
    }
    std::sort(ordered_names.begin(), ordered_names.end());
    for (const auto& net_name : ordered_names) {
        const auto& connections = net_to_connections.at(net_name);
        if (connections.size() >= 2) {
            Net net;
            net.id = net_id_counter++;
            for (const auto& [cell_id, pin_name] : connections) net.cell_ids.push_back(cell_id);
            nets.push_back(net);
            net_names_.push_back(net_name);
            net_connections_.push_back(connections);
        }
    }

    return !cells.empty() && !nets.empty();
}

} // namespace mfe::io
