#include "mfe/io/bookshelf.hpp"
#include <iostream>
#include <fstream>
#include <cassert>
#include <cmath>

void write_temp_file(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "Failed to write temp file: " << path << std::endl;
        std::exit(1);
    }
    out << content;
}

int main() {
    std::cout << "[Bookshelf Parser Test] Creating sample Bookshelf files..." << std::endl;

    std::string nodes_content = 
        "UCLA nodes 1.0\n"
        "# This is a comment\n"
        "NumNodes : 4\n"
        "NumTerminals : 1\n"
        "c0 10.0 20.0\n"
        "c1 5.0 15.0\n"
        "c2 20.0 20.0\n"
        "p0 0.0 0.0 terminal\n";

    std::string pl_content =
        "UCLA pl 1.0\n"
        "c0 100.0 200.0 : N\n"
        "c1 150.0 250.0 : N FIXED\n"
        "c2 200.0 300.0 : N\n"
        "p0 10.0 20.0 : N FIXED\n";

    std::string nets_content =
        "UCLA nets 1.0\n"
        "NumNets : 2\n"
        "NumPins : 5\n"
        "NetDegree : 3 net0\n"
        "  c0 I\n"
        "  c1 O\n"
        "  p0 I\n"
        "NetDegree : 2 net1\n"
        "  c1 I\n"
        "  c2 O\n";

    write_temp_file("temp_test.nodes", nodes_content);
    write_temp_file("temp_test.pl", pl_content);
    write_temp_file("temp_test.nets", nets_content);

    std::cout << "[Bookshelf Parser Test] Parsing layout..." << std::endl;
    mfe::BookshelfLayout layout = mfe::parse_bookshelf("temp_test.nodes", "temp_test.nets", "temp_test.pl");

    std::cout << "[Bookshelf Parser Test] Verifying parsed layout..." << std::endl;

    // Verify cell count
    assert(layout.cells.size() == 4);
    std::cout << "  Cell count verified: " << layout.cells.size() << std::endl;

    // Verify cell mappings and names
    assert(layout.cell_id_to_name[0] == "c0");
    assert(layout.cell_id_to_name[1] == "c1");
    assert(layout.cell_id_to_name[2] == "c2");
    assert(layout.cell_id_to_name[3] == "p0");

    assert(layout.cell_name_to_id["c0"] == 0);
    assert(layout.cell_name_to_id["c1"] == 1);
    assert(layout.cell_name_to_id["c2"] == 2);
    assert(layout.cell_name_to_id["p0"] == 3);

    // Verify masses (width * height)
    assert(std::abs(layout.cells[0].mass - 200.0) < 1e-6); // 10 * 20
    assert(std::abs(layout.cells[1].mass - 75.0) < 1e-6);  // 5 * 15
    assert(std::abs(layout.cells[2].mass - 400.0) < 1e-6); // 20 * 20
    assert(std::abs(layout.cells[3].mass - 0.0) < 1e-6);   // 0 * 0
    std::cout << "  Cell masses verified." << std::endl;

    // Verify positions
    assert(std::abs(layout.cells[0].x - 100.0) < 1e-6 && std::abs(layout.cells[0].y - 200.0) < 1e-6);
    assert(std::abs(layout.cells[1].x - 150.0) < 1e-6 && std::abs(layout.cells[1].y - 250.0) < 1e-6);
    assert(std::abs(layout.cells[2].x - 200.0) < 1e-6 && std::abs(layout.cells[2].y - 300.0) < 1e-6);
    assert(std::abs(layout.cells[3].x - 10.0) < 1e-6 && std::abs(layout.cells[3].y - 20.0) < 1e-6);
    std::cout << "  Cell positions verified." << std::endl;

    // Verify fixed flags
    assert(layout.is_fixed[0] == false); // regular cell, not fixed
    assert(layout.is_fixed[1] == true);  // marked FIXED in .pl
    assert(layout.is_fixed[2] == false); // regular cell, not fixed
    assert(layout.is_fixed[3] == true);  // marked terminal in .nodes and FIXED in .pl
    std::cout << "  Cell fixed flags verified." << std::endl;

    // Verify nets
    assert(layout.nets.size() == 2);
    
    // Net 0
    assert(layout.nets[0].id == 0);
    assert(layout.nets[0].cell_ids.size() == 3);
    assert(layout.nets[0].cell_ids[0] == 0); // c0
    assert(layout.nets[0].cell_ids[1] == 1); // c1
    assert(layout.nets[0].cell_ids[2] == 3); // p0

    // Net 1
    assert(layout.nets[1].id == 1);
    assert(layout.nets[1].cell_ids.size() == 2);
    assert(layout.nets[1].cell_ids[0] == 1); // c1
    assert(layout.nets[1].cell_ids[1] == 2); // c2
    std::cout << "  Nets verified." << std::endl;

    // Clean up
    std::remove("temp_test.nodes");
    std::remove("temp_test.pl");
    std::remove("temp_test.nets");

    std::cout << "[Bookshelf Parser Test] All tests passed successfully!" << std::endl;
    return 0;
}
