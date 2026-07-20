#include "mfe/flow/manifest.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>

namespace mfe::flow {

namespace {

// Trim leading and trailing whitespace
std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

// Strip quotes from string
std::string strip_quotes(const std::string& str) {
    std::string s = trim(str);
    while (!s.empty() && (s.front() == '"' || s.front() == '\'' || s.front() == '[' || s.front() == '{' || s.front() == ' ')) {
        s.erase(0, 1);
    }
    while (!s.empty() && (s.back() == '"' || s.back() == '\'' || s.back() == ']' || s.back() == '}' || s.back() == ',' || s.back() == ' ')) {
        s.pop_back();
    }
    return s;
}

// Extract string value from JSON line like `"key": "value"`
std::string extract_json_value(const std::string& line) {
    auto colon = line.find(':');
    if (colon == std::string::npos) return "";
    std::string val = line.substr(colon + 1);
    // strip trailing comma
    auto comma = val.find_last_of(',');
    if (comma != std::string::npos) {
        val = val.substr(0, comma);
    }
    return strip_quotes(val);
}

// Calculate FNV-1a 64-bit hash of file
std::string calculate_fnv1a(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";

    uint64_t hash = 14695981039346656037ULL;
    const uint64_t prime = 1099511628211ULL;

    char buffer[4096];
    while (file.read(buffer, sizeof(buffer))) {
        std::streamsize bytes = file.gcount();
        for (std::streamsize i = 0; i < bytes; ++i) {
            hash ^= static_cast<uint8_t>(buffer[i]);
            hash *= prime;
        }
    }
    // process remaining bytes
    std::streamsize bytes = file.gcount();
    for (std::streamsize i = 0; i < bytes; ++i) {
        hash ^= static_cast<uint8_t>(buffer[i]);
        hash *= prime;
    }

    std::stringstream ss;
    ss << std::hex << std::setw(16) << std::setfill('0') << hash;
    return ss.str();
}

std::string escape_json(const std::string& value) {
    std::string escaped;
    for (const char character : value) {
        if (character == '"' || character == '\\') escaped += '\\';
        escaped += character;
    }
    return escaped;
}

} // namespace

std::string Finding::serialize_json() const {
    std::stringstream ss;
    ss << "{\n"
       << "      \"id\": \"" << escape_json(id) << "\",\n"
       << "      \"category\": \"" << escape_json(category) << "\",\n"
       << "      \"severity\": \"" << escape_json(severity) << "\",\n"
       << "      \"message\": \"" << escape_json(message) << "\"";
    if (!file.empty()) {
        ss << ",\n      \"source_location\": {\n"
           << "        \"file\": \"" << escape_json(file) << "\",\n"
           << "        \"line\": " << line << "\n"
           << "      }";
    }
    if (!layer.empty()) {
        ss << ",\n      \"geometry_location\": {\n"
           << "        \"layer\": \"" << escape_json(layer) << "\",\n"
           << "        \"x\": " << x << ",\n"
           << "        \"y\": " << y << "\n"
           << "      }";
    }
    ss << "\n    }";
    return ss.str();
}

PdkManifest PdkManifest::load_from_json(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open PDK manifest: " + path.string());
    }

    PdkManifest pdk;
    std::string line;
    bool parsing_cell_lefs = false;
    bool parsing_routing_layers = false;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty()) continue;

        // Reset list states
        if (trimmed.find(']') != std::string::npos) {
            parsing_cell_lefs = false;
            parsing_routing_layers = false;
            continue;
        }

        if (parsing_cell_lefs) {
            std::string lef_path = strip_quotes(trimmed);
            if (!lef_path.empty()) {
                pdk.cell_lefs.push_back(lef_path);
            }
            continue;
        }

        if (parsing_routing_layers) {
            std::string layer = strip_quotes(trimmed);
            if (!layer.empty()) {
                pdk.routing_layers.push_back(layer);
            }
            continue;
        }

        if (trimmed.find("\"name\"") != std::string::npos) {
            pdk.name = extract_json_value(trimmed);
        } else if (trimmed.find("\"dbu_per_micron\"") != std::string::npos) {
            pdk.dbu_per_micron = std::stoi(extract_json_value(trimmed));
        } else if (trimmed.find("\"technology_lef\"") != std::string::npos) {
            pdk.technology_lef = extract_json_value(trimmed);
        } else if (trimmed.find("\"cell_lefs\"") != std::string::npos) {
            parsing_cell_lefs = true;
        } else if (trimmed.find("\"routing_layers\"") != std::string::npos) {
            parsing_routing_layers = true;
        } else if (trimmed.find("\"via_rules\"") != std::string::npos) {
            pdk.via_rules = extract_json_value(trimmed);
        } else if (trimmed.find("\"rcx_rules\"") != std::string::npos) {
            pdk.rcx_rules = extract_json_value(trimmed);
        } else if (trimmed.find("\"layer_map\"") != std::string::npos) {
            pdk.layer_map = extract_json_value(trimmed);
        } else if (trimmed.find("\"drc_deck\"") != std::string::npos) {
            pdk.drc_deck = extract_json_value(trimmed);
        } else if (trimmed.find("\"lvs_deck\"") != std::string::npos) {
            pdk.lvs_deck = extract_json_value(trimmed);
        } else if (trimmed.find("\"typical\"") != std::string::npos) {
            pdk.liberty_corners["typical"] = extract_json_value(trimmed);
        }
    }

    // PDK manifests are portable repository configuration, not machine-local
    // state.  Resolve every relative artifact against its manifest so a clone
    // can live anywhere on disk.
    const auto base = path.parent_path();
    const auto resolve = [&base](std::filesystem::path artifact) {
        return artifact.is_absolute() ? artifact : (base / artifact).lexically_normal();
    };
    pdk.technology_lef = resolve(pdk.technology_lef);
    for (auto& lef : pdk.cell_lefs) lef = resolve(lef);
    for (auto& [corner, liberty] : pdk.liberty_corners) liberty = resolve(liberty);
    pdk.via_rules = resolve(pdk.via_rules);
    pdk.rcx_rules = resolve(pdk.rcx_rules);
    pdk.layer_map = resolve(pdk.layer_map);
    pdk.drc_deck = resolve(pdk.drc_deck);
    pdk.lvs_deck = resolve(pdk.lvs_deck);
    return pdk;
}

bool PdkManifest::validate() const {
    if (name.empty()) return false;
    if (dbu_per_micron <= 0) return false;
    if (!std::filesystem::exists(technology_lef)) return false;
    for (const auto& lef : cell_lefs) {
        if (!std::filesystem::exists(lef)) return false;
    }
    if (liberty_corners.empty()) return false;
    for (const auto& [corner, lib_path] : liberty_corners) {
        if (!std::filesystem::exists(lib_path)) return false;
    }
    return true;
}

FlowProject FlowProject::load_from_yaml(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open project manifest: " + path.string());
    }

    FlowProject project;
    std::string line;
    bool parsing_rtl = false;

    while (std::getline(file, line)) {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#') continue;

        if (trimmed.front() == '-') {
            if (parsing_rtl) {
                std::string r_file = trim(trimmed.substr(1));
                if (!r_file.empty()) {
                    project.rtl.push_back(r_file);
                }
            }
            continue;
        }

        auto colon = trimmed.find(':');
        if (colon == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, colon));
        std::string val = trim(trimmed.substr(colon + 1));

        if (key == "design") {
            project.design = val;
            parsing_rtl = false;
        } else if (key == "top") {
            project.top = val;
            parsing_rtl = false;
        } else if (key == "pdk") {
            project.pdk = val;
            parsing_rtl = false;
        } else if (key == "rtl") {
            parsing_rtl = true;
        } else if (key == "constraints") {
            project.constraints = val;
            parsing_rtl = false;
        } else if (key == "name") {
            project.clock_name = val;
            parsing_rtl = false;
        } else if (key == "period_ns") {
            project.clock_period_ns = std::stod(val);
            parsing_rtl = false;
        } else if (key == "outputs") {
            project.outputs = val;
            parsing_rtl = false;
        }
    }

    return project;
}

bool FlowProject::validate() const {
    if (design.empty()) return false;
    if (top.empty()) return false;
    if (pdk.empty()) return false;
    if (rtl.empty()) return false;
    return true;
}

void ArtifactManifest::add_artifact(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        file_hashes[path.generic_string()] = calculate_fnv1a(path);
    }
}

bool ArtifactManifest::verify_hashes() const {
    for (const auto& [file_path, original_hash] : file_hashes) {
        if (!std::filesystem::exists(file_path)) return false;
        if (calculate_fnv1a(file_path) != original_hash) return false;
    }
    return true;
}

std::string ArtifactManifest::get_hash(const std::filesystem::path& path) const {
    auto it = file_hashes.find(path.generic_string());
    if (it != file_hashes.end()) return it->second;
    return "";
}

void write_stage_result(const std::filesystem::path& path, 
                        const std::string& stage,
                        const std::string& status,
                        const std::vector<std::filesystem::path>& inputs,
                        const std::vector<std::filesystem::path>& outputs,
                        const std::map<std::string, double>& metrics,
                        const std::vector<Finding>& findings) {
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream file(path);
    if (!file) throw std::runtime_error("Failed to write stage results: " + path.string());

    file << "{\n"
         << "  \"stage\": \"" << escape_json(stage) << "\",\n"
         << "  \"status\": \"" << escape_json(status) << "\",\n";

    // Write inputs
    file << "  \"inputs\": [";
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        if (i != 0) file << ", ";
        file << "\"" << escape_json(inputs[i].generic_string()) << "\"";
    }
    file << "],\n";

    // Write outputs
    file << "  \"outputs\": [";
    for (std::size_t i = 0; i < outputs.size(); ++i) {
        if (i != 0) file << ", ";
        file << "\"" << escape_json(outputs[i].generic_string()) << "\"";
    }
    file << "],\n";

    // Write metrics
    file << "  \"metrics\": {\n";
    std::size_t metric_index = 0;
    for (const auto& [name, value] : metrics) {
        if (metric_index != 0) file << ",\n";
        file << "    \"" << escape_json(name) << "\": " << value;
        metric_index++;
    }
    file << "\n  },\n";

    // Write findings
    file << "  \"findings\": [";
    for (std::size_t i = 0; i < findings.size(); ++i) {
        if (i != 0) file << ",\n";
        file << findings[i].serialize_json();
    }
    file << "]\n}\n";
}

} // namespace mfe::flow
