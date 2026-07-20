#pragma once

#include "mfe/core/mesh.hpp"
#include "mfe/diagnostics/diagnostic.hpp"
#include "mfe/io/lef_def.hpp"
#include "mfe/pdk/technology.hpp"

#include <filesystem>

namespace mfe::io {

struct RunArtifacts {
    std::filesystem::path directory;
    std::filesystem::path manifest;
    std::filesystem::path mesh_csv;
    std::filesystem::path diagnostics_json;
};

// Writes a deterministic, self-contained run directory. Throws std::runtime_error on I/O failure.
[[nodiscard]] RunArtifacts write_run_artifacts(const std::filesystem::path& directory,
                                                const pdk::Technology& technology,
                                                const Design& design,
                                                const core::UniformMesh& mesh,
                                                const diagnostics::DiagnosticReport& report);

}  // namespace mfe::io
