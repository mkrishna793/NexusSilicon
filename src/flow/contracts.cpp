#include "mfe/flow/contracts.hpp"

#include <fstream>
#include <stdexcept>

namespace mfe::flow {
namespace {

std::string escape_json(const std::string& value) {
    std::string escaped;
    for (const char character : value) {
        if (character == '"' || character == '\\') escaped += '\\';
        escaped += character;
    }
    return escaped;
}

}  // namespace

const char* stage_name(Stage stage) noexcept {
    switch (stage) {
        case Stage::synthesis: return "synthesis";
        case Stage::floorplan: return "floorplan";
        case Stage::power_delivery: return "power_delivery";
        case Stage::placement: return "placement";
        case Stage::analysis: return "analysis";
        case Stage::clock_tree: return "clock_tree";
        case Stage::global_routing: return "global_routing";
        case Stage::detailed_routing: return "detailed_routing";
        case Stage::extraction: return "extraction";
        case Stage::timing: return "timing";
        case Stage::verification: return "verification";
        case Stage::stream_out: return "stream_out";
    }
    return "unknown";
}

const char* change_target_name(ChangeTarget target) noexcept {
    switch (target) {
        case ChangeTarget::rtl: return "rtl";
        case ChangeTarget::constraints: return "constraints";
        case ChangeTarget::flow_configuration: return "flow_configuration";
        case ChangeTarget::physical_eco: return "physical_eco";
        case ChangeTarget::pdk: return "pdk";
    }
    return "unknown";
}

const char* change_status_name(ChangeStatus status) noexcept {
    switch (status) {
        case ChangeStatus::proposed: return "proposed";
        case ChangeStatus::approved: return "approved";
        case ChangeStatus::rejected: return "rejected";
        case ChangeStatus::applied: return "applied";
    }
    return "unknown";
}

bool is_safe_to_apply(const ChangeSet& change) noexcept {
    return change.target != ChangeTarget::pdk && change.status == ChangeStatus::approved;
}

void write_run_record(const std::filesystem::path& path, const RunRecord& run) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("Unable to write flow run record: " + path.string());

    output << "{\n  \"schema_version\": 1,\n  \"id\": \"" << escape_json(run.id)
           << "\",\n  \"design\": \"" << escape_json(run.design)
           << "\",\n  \"technology\": \"" << escape_json(run.technology) << "\",\n  \"stages\": [";
    for (std::size_t index = 0; index < run.stages.size(); ++index) {
        const auto& stage = run.stages[index];
        if (index != 0) output << ',';
        output << "\n    {\"name\":\"" << stage_name(stage.stage) << "\",\"succeeded\":"
               << (stage.succeeded ? "true" : "false") << ",\"metrics\":[";
        for (std::size_t metric_index = 0; metric_index < stage.metrics.size(); ++metric_index) {
            const auto& metric = stage.metrics[metric_index];
            if (metric_index != 0) output << ',';
            output << "{\"name\":\"" << escape_json(metric.name) << "\",\"value\":" << metric.value
                   << ",\"unit\":\"" << escape_json(metric.unit) << "\"}";
        }
        output << "],\"artifacts\":[";
        for (std::size_t artifact_index = 0; artifact_index < stage.artifacts.size(); ++artifact_index) {
            if (artifact_index != 0) output << ',';
            output << "\"" << escape_json(stage.artifacts[artifact_index].generic_string()) << "\"";
        }
        output << "],\"diagnostic_codes\":[";
        for (std::size_t diagnostic_index = 0; diagnostic_index < stage.diagnostic_codes.size(); ++diagnostic_index) {
            if (diagnostic_index != 0) output << ',';
            output << "\"" << escape_json(stage.diagnostic_codes[diagnostic_index]) << "\"";
        }
        output << "]}";
    }
    output << "\n  ]\n}\n";
}

}  // namespace mfe::flow
