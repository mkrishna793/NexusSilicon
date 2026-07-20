#include "mfe/io/oasis_writer.hpp"
#include <fstream>
#include <iostream>

namespace mfe::io {

namespace {

void write_oasis_varint(std::ofstream& out, uint64_t val) {
    while (val >= 0x80) {
        out.put(static_cast<char>((val & 0x7F) | 0x80));
        val >>= 7;
    }
    out.put(static_cast<char>(val & 0x7F));
}

void write_oasis_string(std::ofstream& out, const std::string& str) {
    write_oasis_varint(out, str.size());
    out.write(str.data(), str.size());
}

} // namespace

bool OasisWriter::write_oasis(const std::filesystem::path& path,
                              const std::string& design_name,
                              const std::vector<Cell>& cells,
                              const std::vector<SourceLink>& links,
                              const std::vector<RoutedNet>& routed_nets,
                              double dbu_per_micron) {
    std::cout << "[OASIS Writer] Exporting binary layout to: " << path.string() << std::endl;
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: Failed to open OASIS file for writing." << std::endl;
        return false;
    }

    // 1. Magic Header
    out.write("%OASIS\r\n", 8);

    // 2. START record (Record ID 1)
    out.put(1); // START ID
    write_oasis_string(out, "1.0"); // Version
    write_oasis_varint(out, 1000);   // DBU table (1000 DBU per micron)
    out.put(0); // Offset flag

    // 3. CELL record (Record ID 13) for design name
    out.put(13); // CELL ID
    write_oasis_string(out, design_name);

    // 4. Write standard cells as RECTANGLE records (Record ID 20) on layer 10
    for (const auto& cell : cells) {
        out.put(20); // RECTANGLE ID
        write_oasis_varint(out, 10); // Layer 10
        write_oasis_varint(out, 0);  // Datatype 0

        int32_t x = static_cast<int32_t>(cell.x * dbu_per_micron);
        int32_t y = static_cast<int32_t>(cell.y * dbu_per_micron);
        int32_t w = static_cast<int32_t>(10.0 * dbu_per_micron);
        int32_t h = static_cast<int32_t>(10.0 * dbu_per_micron);

        write_oasis_varint(out, x);
        write_oasis_varint(out, y);
        write_oasis_varint(out, w);
        write_oasis_varint(out, h);
    }

    // 5. Write routed nets as PATH records (Record ID 21) on corresponding layers
    for (const auto& net : routed_nets) {
        for (const auto& seg : net.segments) {
            out.put(21); // PATH ID
            
            uint64_t layer_num = 31;
            if (seg.layer == "M2") layer_num = 32;
            else if (seg.layer == "VIA12") layer_num = 41;

            write_oasis_varint(out, layer_num);
            write_oasis_varint(out, 0); // Datatype 0

            int32_t x1 = static_cast<int32_t>(seg.x1 * dbu_per_micron);
            int32_t y1 = static_cast<int32_t>(seg.y1 * dbu_per_micron);
            int32_t x2 = static_cast<int32_t>(seg.x2 * dbu_per_micron);
            int32_t y2 = static_cast<int32_t>(seg.y2 * dbu_per_micron);

            write_oasis_varint(out, x1);
            write_oasis_varint(out, y1);
            write_oasis_varint(out, x2);
            write_oasis_varint(out, y2);
        }
    }

    // 6. END record (Record ID 2)
    out.put(2);

    out.close();
    std::cout << "[OASIS Writer] Exported successfully." << std::endl;
    return true;
}

} // namespace mfe::io
