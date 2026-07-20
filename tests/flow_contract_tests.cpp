#include "mfe/flow/contracts.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

int main() {
    using namespace mfe::flow;

    const ChangeSet pdk_change{"pdk-change", ChangeTarget::pdk, ChangeStatus::approved, "unsafe", {}, {}};
    const ChangeSet approved_flow_change{"flow-change", ChangeTarget::flow_configuration, ChangeStatus::approved, "safe", {}, {}};
    assert(!is_safe_to_apply(pdk_change));
    assert(is_safe_to_apply(approved_flow_change));

    const auto path = std::filesystem::temp_directory_path() / "mfe-flow-contract-test.json";
    RunRecord run{"run-001", "gcd", "asap7", {{Stage::placement, true, {{"utilization", 0.42, "ratio"}}, {"mesh.csv"}, {"MFE-CONG-001"}}}};
    write_run_record(path, run);

    std::string json;
    {
        std::ifstream input(path);
        json.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }
    assert(json.find("\"placement\"") != std::string::npos);
    assert(json.find("MFE-CONG-001") != std::string::npos);
    std::filesystem::remove(path);
    std::cout << "All MFE flow contract tests passed.\n";
}
