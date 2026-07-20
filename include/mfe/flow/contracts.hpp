#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mfe::flow {

// A stable vocabulary shared by native MFE engines, external adapters, and the AI layer.
enum class Stage {
    synthesis,
    floorplan,
    power_delivery,
    placement,
    analysis,
    clock_tree,
    global_routing,
    detailed_routing,
    extraction,
    timing,
    verification,
    stream_out,
};

enum class ChangeTarget { rtl, constraints, flow_configuration, physical_eco, pdk };
enum class ChangeStatus { proposed, approved, rejected, applied };

struct Metric {
    std::string name;
    double value{0.0};
    std::string unit;
};

struct StageResult {
    Stage stage{Stage::synthesis};
    bool succeeded{false};
    std::vector<Metric> metrics;
    std::vector<std::filesystem::path> artifacts;
    std::vector<std::string> diagnostic_codes;
};

// The agent can propose changes only through this object. PDK edits remain forbidden.
struct ChangeSet {
    std::string id;
    ChangeTarget target{ChangeTarget::flow_configuration};
    ChangeStatus status{ChangeStatus::proposed};
    std::string rationale;
    std::vector<std::string> operations;
    std::vector<Stage> rerun_from;
};

struct ImpactAssessment {
    std::string change_set_id;
    std::vector<Stage> affected_stages;
    std::vector<std::string> expected_metric_changes;
    std::vector<std::string> risks;
};

struct ExperimentRecord {
    std::string id;
    std::string base_run_id;
    std::string change_set_id;
    bool approved_by_engineer{false};
    std::vector<StageResult> results;
};

struct RunRecord {
    std::string id;
    std::string design;
    std::string technology;
    std::vector<StageResult> stages;
};

[[nodiscard]] const char* stage_name(Stage stage) noexcept;
[[nodiscard]] const char* change_target_name(ChangeTarget target) noexcept;
[[nodiscard]] const char* change_status_name(ChangeStatus status) noexcept;

// Enforces the safety boundary: proposed PDK changes are never executable by the flow.
[[nodiscard]] bool is_safe_to_apply(const ChangeSet& change) noexcept;

// Writes a machine-readable run record for the dashboard and AI evidence graph.
void write_run_record(const std::filesystem::path& path, const RunRecord& run);

}  // namespace mfe::flow
