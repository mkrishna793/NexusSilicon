#include "mfe/flow/evidence_graph.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

int main() {
    using namespace mfe::flow;
    EvidenceGraph graph;
    graph.add_entity({"rtl:alu.add", EntityKind::rtl_symbol, "alu add", {{"rtl/alu.sv", 21, 5, 24, 1}}});
    graph.add_entity({"net:sum", EntityKind::net, "sum", std::nullopt});
    graph.add_entity({"finding:timing-1", EntityKind::finding, "setup violation", std::nullopt});
    graph.add_edge({"rtl:alu.add", "maps_to", "net:sum"});
    graph.add_edge({"net:sum", "contributes_to", "finding:timing-1"});

    assert(graph.find("net:sum") != nullptr);
    assert(graph.outgoing("net:sum").size() == 1);
    assert(graph.incoming("net:sum").size() == 1);
    assert(graph.at_source("rtl/alu.sv", 23).size() == 1);
    assert(graph.neighborhood("finding:timing-1", 2, 8).size() == 3);
    const auto causal_path = graph.shortest_path("finding:timing-1", "rtl:alu.add", 3);
    assert(causal_path.size() == 3);
    assert(causal_path.front() == "finding:timing-1");
    assert(causal_path.back() == "rtl:alu.add");

    const auto path = std::filesystem::temp_directory_path() / "mfe-evidence-graph-test.json";
    write_evidence_graph(path, graph);
    std::string json;
    { std::ifstream input(path); json.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()); }
    assert(json.find("rtl/alu.sv") != std::string::npos);
    assert(json.find("contributes_to") != std::string::npos);
    std::filesystem::remove(path);
    std::cout << "All MFE evidence graph tests passed.\n";
}
