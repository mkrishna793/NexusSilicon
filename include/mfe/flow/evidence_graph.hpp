#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace mfe::flow {

enum class EntityKind {
    rtl_symbol,
    constraint,
    cell,
    instance,
    net,
    route,
    timing_path,
    pdk_rule,
    finding,
    artifact,
};

struct SourceLocation {
    std::filesystem::path file;
    std::size_t line{0};
    std::size_t column{0};
    std::size_t end_line{0};
    std::size_t end_column{0};
};

struct EvidenceEntity {
    std::string id;
    EntityKind kind{EntityKind::artifact};
    std::string display_name;
    std::optional<SourceLocation> source;
};

struct EvidenceEdge {
    std::string from;
    std::string relation;
    std::string to;
};

// Cross-stage, tool-independent design evidence. Adapters populate this graph; the AI only queries it.
class EvidenceGraph {
public:
    void add_entity(EvidenceEntity entity);
    void add_edge(EvidenceEdge edge);

    [[nodiscard]] const EvidenceEntity* find(const std::string& id) const noexcept;
    [[nodiscard]] std::vector<EvidenceEdge> outgoing(const std::string& id) const;
    [[nodiscard]] std::vector<EvidenceEdge> incoming(const std::string& id) const;
    [[nodiscard]] std::vector<const EvidenceEntity*> at_source(const std::filesystem::path& file,
                                                                std::size_t line) const;
    // Bounded bidirectional traversal for agent investigations. The result never includes unrelated design data.
    [[nodiscard]] std::vector<const EvidenceEntity*> neighborhood(const std::string& root_id,
                                                                    std::size_t max_depth,
                                                                    std::size_t max_entities) const;
    // Returns an ordered causal chain such as finding -> timing path -> net -> RTL symbol.
    [[nodiscard]] std::vector<std::string> shortest_path(const std::string& from, const std::string& to,
                                                          std::size_t max_depth) const;
    [[nodiscard]] const std::vector<EvidenceEntity>& entities() const noexcept;
    [[nodiscard]] const std::vector<EvidenceEdge>& edges() const noexcept;

private:
    std::vector<EvidenceEntity> entities_;
    std::vector<EvidenceEdge> edges_;
};

[[nodiscard]] const char* entity_kind_name(EntityKind kind) noexcept;
void write_evidence_graph(const std::filesystem::path& path, const EvidenceGraph& graph);

}  // namespace mfe::flow
