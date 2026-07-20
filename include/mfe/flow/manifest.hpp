#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

namespace mfe::flow {

struct Finding {
    std::string id;
    std::string category; // timing, drc, lvs, synthesis, power, geometry
    std::string severity; // error, warning, info
    std::string message;
    std::string file;
    int line{-1};
    std::string layer;
    double x{0.0};
    double y{0.0};

    [[nodiscard]] std::string serialize_json() const;
};

struct PdkManifest {
    std::string name;
    int dbu_per_micron{1000};
    std::filesystem::path technology_lef;
    std::vector<std::filesystem::path> cell_lefs;
    std::map<std::string, std::filesystem::path> liberty_corners;
    std::vector<std::string> routing_layers;
    std::filesystem::path via_rules;
    std::filesystem::path rcx_rules;
    std::filesystem::path layer_map;
    std::filesystem::path drc_deck;
    std::filesystem::path lvs_deck;

    // Load PDK configuration from manifest.json
    [[nodiscard]] static PdkManifest load_from_json(const std::filesystem::path& path);
    // Verifies all required PDK files exist
    [[nodiscard]] bool validate() const;
};

struct FlowProject {
    std::string design;
    std::string top;
    std::string pdk;
    std::vector<std::filesystem::path> rtl;
    std::filesystem::path constraints;
    std::string clock_name;
    double clock_period_ns{1.0};
    std::filesystem::path outputs;

    // Load design configuration from project.yaml
    [[nodiscard]] static FlowProject load_from_yaml(const std::filesystem::path& path);
    [[nodiscard]] bool validate() const;
};

struct ArtifactManifest {
    std::unordered_map<std::string, std::string> file_hashes;

    void add_artifact(const std::filesystem::path& path);
    [[nodiscard]] bool verify_hashes() const;
    [[nodiscard]] std::string get_hash(const std::filesystem::path& path) const;
};

// Writes standardized stage result log
void write_stage_result(const std::filesystem::path& path, 
                        const std::string& stage,
                        const std::string& status,
                        const std::vector<std::filesystem::path>& inputs,
                        const std::vector<std::filesystem::path>& outputs,
                        const std::map<std::string, double>& metrics,
                        const std::vector<Finding>& findings);

} // namespace mfe::flow
