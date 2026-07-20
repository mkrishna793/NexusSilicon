#include "mfe/io/artifacts.hpp"

#include <fstream>
#include <iomanip>
#include <stdexcept>

namespace mfe::io {
namespace {

std::string escape_json(const std::string& text) {
    std::string escaped;
    for (const char ch : text) { if (ch == '"' || ch == '\\') escaped += '\\'; escaped += ch; }
    return escaped;
}

void ensure_open(const std::ofstream& stream, const std::filesystem::path& path) {
    if (!stream) throw std::runtime_error("Unable to write artifact: " + path.string());
}

const char* severity_name(diagnostics::Severity severity) {
    switch (severity) {
        case diagnostics::Severity::info: return "info";
        case diagnostics::Severity::warning: return "warning";
        case diagnostics::Severity::error: return "error";
        case diagnostics::Severity::critical: return "critical";
    }
    return "unknown";
}

}  // namespace

RunArtifacts write_run_artifacts(const std::filesystem::path& directory, const pdk::Technology& technology,
                                 const Design& design, const core::UniformMesh& mesh,
                                 const diagnostics::DiagnosticReport& report) {
    std::filesystem::create_directories(directory);
    RunArtifacts artifacts{directory, directory / "manifest.json", directory / "mesh.csv", directory / "diagnostics.json"};
    std::ofstream manifest(artifacts.manifest); ensure_open(manifest, artifacts.manifest);
    manifest << "{\n  \"schema_version\": 1,\n  \"technology\": \"" << escape_json(technology.name())
             << "\",\n  \"design\": \"" << escape_json(design.name) << "\",\n  \"mesh\": {\"columns\": "
             << mesh.columns() << ", \"rows\": " << mesh.rows() << "},\n  \"files\": [\"mesh.csv\", \"diagnostics.json\"]\n}\n";
    std::ofstream csv(artifacts.mesh_csv); ensure_open(csv, artifacts.mesh_csv);
    csv << "column,row,left,bottom,right,top,demand,capacity,utilization\n" << std::setprecision(17);
    for (std::size_t row = 0; row < mesh.rows(); ++row) for (std::size_t column = 0; column < mesh.columns(); ++column) {
        const auto& cell = mesh.cell(column, row); const auto utilization = cell.capacity > 0 ? cell.demand / cell.capacity : 0.0;
        csv << column << ',' << row << ',' << cell.bounds.left << ',' << cell.bounds.bottom << ',' << cell.bounds.right << ','
            << cell.bounds.top << ',' << cell.demand << ',' << cell.capacity << ',' << utilization << '\n';
    }
    std::ofstream diagnostics(artifacts.diagnostics_json); ensure_open(diagnostics, artifacts.diagnostics_json);
    diagnostics << "{\n  \"subject\": \"" << escape_json(report.subject) << "\",\n  \"findings\": [";
    for (std::size_t i = 0; i < report.findings.size(); ++i) {
        const auto& finding = report.findings[i]; if (i) diagnostics << ',';
        diagnostics << "\n    {\"code\":\"" << escape_json(finding.code) << "\",\"severity\":\"" << severity_name(finding.severity)
                    << "\",\"location\":[" << finding.location.left << ',' << finding.location.bottom << ',' << finding.location.right << ',' << finding.location.top
                    << "],\"cause\":\"" << escape_json(finding.cause) << "\",\"recommendation\":\"" << escape_json(finding.recommendation)
                    << "\",\"confidence\":" << finding.confidence << '}';
    }
    diagnostics << "\n  ]\n}\n";
    return artifacts;
}

}  // namespace mfe::io
