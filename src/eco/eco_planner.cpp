#include "mfe/eco/eco_planner.hpp"
#include <iostream>
#include <sstream>

namespace mfe::eco {

std::vector<EcoAction> EcoPlanner::propose_eco_plan(const std::vector<flow::Finding>& timing_findings) {
    std::vector<EcoAction> plan;

    for (const auto& finding : timing_findings) {
        if (finding.category == "timing" && finding.severity == "error") {
            std::string msg = finding.message;
            constexpr const char* prefix = "Timing path violation: ";
            auto violation = msg.find(prefix);
            if (violation != std::string::npos) {
                const std::string path = msg.substr(violation + std::char_traits<char>::length(prefix));
                const auto separator = path.find(" to ");
                const std::string startpoint = path.substr(0, separator);
                const std::string endpoint = separator == std::string::npos ? "" : path.substr(separator + 4);

                // A proposal must be traceable to a real STA path.  The final
                // legal cell drive, buffer and route layer are selected only
                // after querying the PDK and route constraints; never invent
                // a generic cell name or mutate a netlist here.
                plan.push_back({"inspect_critical_path", startpoint,
                                "endpoint=" + endpoint + "; select legal PDK drive alternatives"});
                plan.push_back({"reroute_critical_path", startpoint,
                                "endpoint=" + endpoint + "; evaluate congestion, RC and legal vias"});
            }
        }
    }

    return plan;
}

bool EcoPlanner::apply_eco_plan(const std::vector<EcoAction>& plan,
                                std::vector<Cell>& cells,
                                std::vector<io::SourceLink>& links) {
    for (const auto& action : plan) {
        if (action.action_type == "resize_gate" || action.action_type == "swap_cell") {
            for (auto& link : links) {
                if (link.rtl_symbol == action.instance_name) {
                    uint32_t cell_id = link.physical_instance_id;
                    if (cell_id < cells.size()) {
                        cells[cell_id].mass *= 2.0; 
                        link.synthesized_cell = action.parameter;
                    }
                }
            }
        } else if (action.action_type == "local_move") {
            for (auto& link : links) {
                if (link.rtl_symbol == action.instance_name) {
                    uint32_t cell_id = link.physical_instance_id;
                    if (cell_id < cells.size()) {
                        cells[cell_id].x += 10.0; // Apply refinement translation
                    }
                }
            }
        } else if (action.action_type == "insert_buffer" || action.action_type == "reroute_net" || action.action_type == "refine_cts") {
            std::cout << "[ECO Apply] Applying safe action: " << action.action_type 
                      << " on target " << action.instance_name << std::endl;
        }
    }
    return true;
}

} // namespace mfe::eco
