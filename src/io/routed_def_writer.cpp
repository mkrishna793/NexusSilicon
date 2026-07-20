#include "mfe/io/routed_def_writer.hpp"

#include <fstream>
#include <iostream>
#include <iomanip>
#include <unordered_map>

namespace mfe::io {

bool write_routed_def(const std::filesystem::path& path,
                      const std::string& design_name,
                      const std::vector<Cell>& cells,
                      const std::vector<SourceLink>& links,
                      const std::vector<RoutedNet>& routed_nets,
                      int dbu_per_micron) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open output DEF file: " << path.string() << std::endl;
        return false;
    }

    // Map cell ID to their SourceLink info for name mapping
    std::unordered_map<uint32_t, SourceLink> link_map;
    for (const auto& link : links) {
        link_map[link.physical_instance_id] = link;
    }

    // Determine die boundaries dynamically
    double max_x_um = 100.0;
    double max_y_um = 100.0;
    for (const auto& cell : cells) {
        if (cell.x > max_x_um) max_x_um = cell.x;
        if (cell.y > max_y_um) max_y_um = cell.y;
    }
    // Add margin
    max_x_um += 10.0;
    max_y_um += 10.0;

    int max_x_dbu = static_cast<int>(max_x_um * dbu_per_micron);
    int max_y_dbu = static_cast<int>(max_y_um * dbu_per_micron);

    file << "VERSION 5.8 ;\n"
         << "NAMESCASESENSITIVE ON ;\n"
         << "DIVIDERCHAR \"/\" ;\n"
         << "BUSBITCHARS \"[]\" ;\n"
         << "DESIGN " << design_name << " ;\n"
         << "UNITS DISTANCE MICRONS " << dbu_per_micron << " ;\n\n"
         << "DIEAREA ( 0 0 ) ( " << max_x_dbu << " " << max_y_dbu << " ) ;\n\n";

    // Write COMPONENTS
    file << "COMPONENTS " << cells.size() << " ;\n";
    for (const auto& cell : cells) {
        std::string inst_name = "inst_" + std::to_string(cell.id);
        std::string cell_type = "AND2x1";

        auto it = link_map.find(cell.id);
        if (it != link_map.end()) {
            inst_name = it->second.rtl_symbol;
            cell_type = it->second.synthesized_cell;
        }

        int x_dbu = static_cast<int>(cell.x * dbu_per_micron);
        int y_dbu = static_cast<int>(cell.y * dbu_per_micron);

        file << "  - " << inst_name << " " << cell_type << " + PLACED ( " 
             << x_dbu << " " << y_dbu << " ) N ;\n";
    }
    file << "END COMPONENTS\n\n";

    // Write NETS
    file << "NETS " << routed_nets.size() << " ;\n";
    for (const auto& net : routed_nets) {
        file << "  - " << net.name << "\n";
        
        // Connections
        file << "    ";
        for (const auto& conn : net.connections) {
            file << "( " << conn.first << " " << conn.second << " ) ";
        }
        file << "\n";

        // DEF terminates a net once.  Additional segments must be appended with
        // NEW, rather than starting another `+ ROUTED` statement after `;`.
        bool first_segment = true;
        for (const auto& seg : net.segments) {
            int x1_dbu = static_cast<int>(seg.x1 * dbu_per_micron);
            int y1_dbu = static_cast<int>(seg.y1 * dbu_per_micron);
            int x2_dbu = static_cast<int>(seg.x2 * dbu_per_micron);
            int y2_dbu = static_cast<int>(seg.y2 * dbu_per_micron);

            file << (first_segment ? "    + ROUTED " : " NEW ") << seg.layer << " ( " 
                 << x1_dbu << " " << y1_dbu << " ) ( " 
                 << x2_dbu << " " << y2_dbu << " )";
            first_segment = false;
        }
        if (!net.segments.empty()) file << " ;";
        file << "\n";
    }
    file << "END NETS\n\n";

    file << "END DESIGN\n";
    file.close();
    return true;
}

} // namespace mfe::io
