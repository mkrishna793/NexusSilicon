#include "mfe/io/bookshelf.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>

namespace mfe {

BookshelfLayout parse_bookshelf(const std::string& nodes_path,
                                const std::string& nets_path,
                                const std::string& pl_path) {
    BookshelfLayout layout;

    // 1. Parse .nodes file
    {
        std::ifstream file(nodes_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open nodes file: " + nodes_path);
        }

        std::string line;
        while (std::getline(file, line)) {
            // strip comment
            auto comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }
            std::stringstream ss(line);
            std::string token;
            if (!(ss >> token)) continue;

            if (token == "UCLA" || token == "NumNodes" || token == "NumTerminals" || token == ":") {
                continue;
            }

            std::string cell_name = token;
            double width = 0.0;
            double height = 0.0;
            if (!(ss >> width >> height)) {
                continue;
            }

            // Determine if it is a terminal node
            std::string type;
            bool is_terminal = false;
            if (ss >> type) {
                if (type == "terminal" || type == "terminal_NI") {
                    is_terminal = true;
                }
            }

            uint32_t id = static_cast<uint32_t>(layout.cells.size());
            Cell cell;
            cell.id = id;
            cell.mass = width * height;
            cell.x = 0.0;
            cell.y = 0.0;
            cell.weight = 0.0;
            cell.actual_area = 0.0;
            cell.centroid_x = 0.0;
            cell.centroid_y = 0.0;

            layout.cells.push_back(cell);
            layout.cell_name_to_id[cell_name] = id;
            layout.cell_id_to_name.push_back(cell_name);
            layout.is_fixed.push_back(is_terminal); // terminals default to fixed
        }
    }

    // 2. Parse .pl file to set positions and update fixed flags
    {
        std::ifstream file(pl_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open placement file: " + pl_path);
        }

        std::string line;
        while (std::getline(file, line)) {
            auto comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }
            std::stringstream ss(line);
            std::string token;
            if (!(ss >> token)) continue;

            if (token == "UCLA" || token == "pl" || token == ":") {
                continue;
            }

            std::string cell_name = token;
            double x = 0.0;
            double y = 0.0;
            if (!(ss >> x >> y)) {
                continue;
            }

            // Look for FIXED / FIXED_NI in orientation or after colon
            bool fixed = false;
            std::string next_token;
            while (ss >> next_token) {
                if (next_token == "FIXED" || next_token == "FIXED_NI") {
                    fixed = true;
                    break;
                }
            }

            auto it = layout.cell_name_to_id.find(cell_name);
            if (it != layout.cell_name_to_id.end()) {
                uint32_t id = it->second;
                layout.cells[id].x = x;
                layout.cells[id].y = y;
                // If it was declared terminal in .nodes or FIXED in .pl, mark as fixed
                layout.is_fixed[id] = layout.is_fixed[id] || fixed;
            }
        }
    }

    // 3. Parse .nets file
    {
        std::ifstream file(nets_path);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open nets file: " + nets_path);
        }

        std::string line;
        uint32_t current_net_id = 0;
        Net current_net;
        bool parsing_net = false;
        int expected_pins = 0;

        while (std::getline(file, line)) {
            auto comment_pos = line.find('#');
            if (comment_pos != std::string::npos) {
                line = line.substr(0, comment_pos);
            }
            std::stringstream ss(line);
            std::string token;
            if (!(ss >> token)) continue;

            if (token == "UCLA" || token == "NumNets" || token == "NumPins" || token == ":") {
                continue;
            }

            if (token == "NetDegree") {
                if (parsing_net && !current_net.cell_ids.empty()) {
                    layout.nets.push_back(current_net);
                }
                std::string sep;
                int degree = 0;
                ss >> sep;
                if (sep == ":") {
                    ss >> degree;
                } else {
                    degree = std::stoi(sep);
                }
                current_net = Net{};
                current_net.id = current_net_id++;
                parsing_net = true;
                expected_pins = degree;
            } else if (parsing_net) {
                std::string node_name = token;
                auto it = layout.cell_name_to_id.find(node_name);
                if (it != layout.cell_name_to_id.end()) {
                    current_net.cell_ids.push_back(it->second);
                }
                if (current_net.cell_ids.size() == static_cast<size_t>(expected_pins)) {
                    layout.nets.push_back(current_net);
                    parsing_net = false;
                }
            }
        }
        if (parsing_net && !current_net.cell_ids.empty()) {
            layout.nets.push_back(current_net);
        }
    }

    return layout;
}

} // namespace mfe
