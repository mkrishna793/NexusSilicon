#include "mfe/io/gdsii_writer.hpp"
#include <fstream>
#include <iostream>

namespace mfe::io {

namespace {

void write_gds_record(std::ofstream& out, uint8_t type, uint8_t datatype, const std::vector<uint8_t>& data) {
    uint16_t length = 4 + data.size();
    // Big-Endian output
    uint8_t header[4];
    header[0] = (length >> 8) & 0xFF;
    header[1] = length & 0xFF;
    header[2] = type;
    header[3] = datatype;
    out.write(reinterpret_cast<const char*>(header), 4);
    if (!data.empty()) {
        out.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
}

void write_gds_string(std::ofstream& out, uint8_t type, const std::string& str) {
    std::vector<uint8_t> data(str.begin(), str.end());
    if (data.size() % 2 != 0) {
        data.push_back(0); // Pad with null byte to satisfy 2-byte boundary alignments
    }
    write_gds_record(out, type, 0x06, data); // Type 0x06 is ASCII string
}

void write_gds_int16(std::ofstream& out, uint8_t type, const std::vector<int16_t>& vals) {
    std::vector<uint8_t> data;
    for (auto val : vals) {
        data.push_back((val >> 8) & 0xFF);
        data.push_back(val & 0xFF);
    }
    write_gds_record(out, type, 0x02, data); // Type 0x02 is 2-byte signed integer
}

void write_gds_int32(std::ofstream& out, uint8_t type, const std::vector<int32_t>& vals) {
    std::vector<uint8_t> data;
    for (auto val : vals) {
        data.push_back((val >> 24) & 0xFF);
        data.push_back((val >> 16) & 0xFF);
        data.push_back((val >> 8) & 0xFF);
        data.push_back(val & 0xFF);
    }
    write_gds_record(out, type, 0x03, data); // Type 0x03 is 4-byte signed integer
}

} // namespace

bool GdsiiWriter::write_gdsii(const std::filesystem::path& path,
                              const std::string& design_name,
                              const std::vector<Cell>& cells,
                              const std::vector<SourceLink>& links,
                              const std::vector<RoutedNet>& routed_nets,
                              double dbu_per_micron) {
    std::cout << "[GDSII Writer] Exporting binary layout to: " << path.string() << std::endl;
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        std::cerr << "Error: Failed to open GDSII file for writing." << std::endl;
        return false;
    }

    // 1. HEADER (GDSII version 3/6)
    write_gds_int16(out, 0x00, {6}); // 0x00: HEADER

    // 2. BGNLIB (date records)
    write_gds_int16(out, 0x01, {2026, 7, 17, 10, 54, 0, 2026, 7, 17, 10, 54, 0}); // 0x01: BGNLIB

    // 3. LIBNAME
    write_gds_string(out, 0x02, "MFE_LIB");

    // 4. UNITS (DBU to physical: user units 1.0e-6, database units 2.5e-10)
    // We mock double reals with IEEE 754 representations (typical representation data payload)
    std::vector<uint8_t> units_data = {
        0x3E, 0x0A, 0x14, 0x7A, 0xE1, 0x47, 0xAE, 0x14, // 0.0001
        0x3B, 0x44, 0x87, 0x6E, 0xBF, 0x30, 0xBE, 0xE5  // 2.5e-10
    };
    write_gds_record(out, 0x03, 0x05, units_data); // 0x03: UNITS (8-byte real)

    // 5. BGNSTR
    write_gds_int16(out, 0x05, {2026, 7, 17, 10, 54, 0, 2026, 7, 17, 10, 54, 0});

    // 6. STRNAME
    write_gds_string(out, 0x06, design_name);

    // 7. Write standard cell boundary boxes as GDSII boundary elements (on layer 10)
    for (const auto& cell : cells) {
        write_gds_record(out, 0x08, 0x00, {}); // 0x08: BOUNDARY element start
        write_gds_int16(out, 0x0D, {10});     // 0x0D: LAYER 10 (cells layout)
        write_gds_int16(out, 0x0E, {0});      // 0x0E: DATATYPE 0

        // Write polygon boundary coordinates (closed loop: 5 points)
        int32_t x1 = static_cast<int32_t>(cell.x * dbu_per_micron);
        int32_t y1 = static_cast<int32_t>(cell.y * dbu_per_micron);
        double width_um = 1.0;
        double height_um = 1.0;
        for (const auto& link : links) {
            if (link.physical_instance_id == cell.id) {
                width_um = link.width_um;
                height_um = link.height_um;
                break;
            }
        }
        int32_t x2 = x1 + static_cast<int32_t>(width_um * dbu_per_micron);
        int32_t y2 = y1 + static_cast<int32_t>(height_um * dbu_per_micron);

        write_gds_int32(out, 0x10, {x1, y1, x2, y1, x2, y2, x1, y2, x1, y1}); // 0x10: XY coordinates
        write_gds_record(out, 0x11, 0x00, {}); // 0x11: ENDEL
    }

    // 8. Write routed wire segments as PATH elements (on layers matching metal layer numbers)
    for (const auto& net : routed_nets) {
        for (const auto& seg : net.segments) {
            write_gds_record(out, 0x09, 0x00, {}); // 0x09: PATH element start
            
            // Map layer name to layer number
            // Sky130 GDS layer map: li1=67, met1..met5=68..72.
            int16_t layer_num = 68;
            if (seg.layer == "li1") layer_num = 67;
            else if (seg.layer == "met2") layer_num = 69;
            else if (seg.layer == "met3") layer_num = 70;
            else if (seg.layer == "met4") layer_num = 71;
            else if (seg.layer == "met5") layer_num = 72;
            // Preserve legacy layer spellings for older test fixtures.
            else if (seg.layer == "M2") layer_num = 69;

            write_gds_int16(out, 0x0D, {layer_num}); // LAYER
            write_gds_int16(out, 0x0E, {0});         // DATATYPE
            write_gds_int16(out, 0x21, {0});         // 0x21: PATHTYPE 0

            // Width of route (e.g. 0.2 microns)
            int32_t w = static_cast<int32_t>(0.2 * dbu_per_micron);
            write_gds_int32(out, 0x0F, {w});         // 0x0F: WIDTH

            int32_t x1 = static_cast<int32_t>(seg.x1 * dbu_per_micron);
            int32_t y1 = static_cast<int32_t>(seg.y1 * dbu_per_micron);
            int32_t x2 = static_cast<int32_t>(seg.x2 * dbu_per_micron);
            int32_t y2 = static_cast<int32_t>(seg.y2 * dbu_per_micron);

            write_gds_int32(out, 0x10, {x1, y1, x2, y2}); // XY coordinates
            write_gds_record(out, 0x11, 0x00, {});        // ENDEL
        }
    }

    // 9. ENDSTR & ENDLIB
    write_gds_record(out, 0x07, 0x00, {}); // 0x07: ENDSTR
    write_gds_record(out, 0x04, 0x00, {}); // 0x04: ENDLIB

    out.close();
    std::cout << "[GDSII Writer] Exported successfully." << std::endl;
    return true;
}

} // namespace mfe::io
