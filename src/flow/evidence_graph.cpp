#include "mfe/flow/evidence_graph.hpp"

#include <algorithm>
#include <deque>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
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

const char* entity_kind_name(EntityKind kind) noexcept {
    switch (kind) {
        case EntityKind::rtl_symbol: return "rtl_symbol";
        case EntityKind::constraint: return "constraint";
        case EntityKind::cell: return "cell";
        case EntityKind::instance: return "instance";
        case EntityKind::net: return "net";
        case EntityKind::route: return "route";
        case EntityKind::timing_path: return "timing_path";
        case EntityKind::pdk_rule: return "pdk_rule";
        case EntityKind::finding: return "finding";
        case EntityKind::artifact: return "artifact";
    }
    return "unknown";
}

void EvidenceGraph::add_entity(EvidenceEntity entity) {
    if (entity.id.empty()) throw std::invalid_argument("Evidence entity id must not be empty");
    if (find(entity.id) != nullptr) throw std::invalid_argument("Duplicate evidence entity id: " + entity.id);
    entities_.push_back(std::move(entity));
}

void EvidenceGraph::add_edge(EvidenceEdge edge) {
    if (find(edge.from) == nullptr || find(edge.to) == nullptr) {
        throw std::invalid_argument("Evidence edge references an unknown entity");
    }
    if (edge.relation.empty()) throw std::invalid_argument("Evidence edge relation must not be empty");
    edges_.push_back(std::move(edge));
}

const EvidenceEntity* EvidenceGraph::find(const std::string& id) const noexcept {
    const auto iterator = std::find_if(entities_.begin(), entities_.end(), [&id](const EvidenceEntity& entity) {
        return entity.id == id;
    });
    return iterator == entities_.end() ? nullptr : &*iterator;
}

std::vector<EvidenceEdge> EvidenceGraph::outgoing(const std::string& id) const {
    std::vector<EvidenceEdge> result;
    for (const auto& edge : edges_) if (edge.from == id) result.push_back(edge);
    return result;
}

std::vector<EvidenceEdge> EvidenceGraph::incoming(const std::string& id) const {
    std::vector<EvidenceEdge> result;
    for (const auto& edge : edges_) if (edge.to == id) result.push_back(edge);
    return result;
}

std::vector<const EvidenceEntity*> EvidenceGraph::at_source(const std::filesystem::path& file,
                                                             std::size_t line) const {
    std::vector<const EvidenceEntity*> result;
    const auto normalized_file = file.lexically_normal().generic_string();
    for (const auto& entity : entities_) {
        if (!entity.source.has_value()) continue;
        const auto& source = *entity.source;
        const auto source_file = source.file.lexically_normal().generic_string();
        const auto end_line = source.end_line == 0 ? source.line : source.end_line;
        if (source_file == normalized_file && line >= source.line && line <= end_line) result.push_back(&entity);
    }
    return result;
}

std::vector<const EvidenceEntity*> EvidenceGraph::neighborhood(const std::string& root_id, std::size_t max_depth,
                                                                std::size_t max_entities) const {
    if (find(root_id) == nullptr || max_entities == 0) return {};
    std::vector<const EvidenceEntity*> result;
    std::unordered_set<std::string> visited{root_id};
    std::deque<std::pair<std::string, std::size_t>> queue{{root_id, 0}};
    while (!queue.empty() && result.size() < max_entities) {
        const auto [current, depth] = queue.front();
        queue.pop_front();
        result.push_back(find(current));
        if (depth == max_depth) continue;
        for (const auto& edge : edges_) {
            const auto& adjacent = edge.from == current ? edge.to : (edge.to == current ? edge.from : std::string{});
            if (!adjacent.empty() && visited.insert(adjacent).second) queue.emplace_back(adjacent, depth + 1);
        }
    }
    return result;
}

std::vector<std::string> EvidenceGraph::shortest_path(const std::string& from, const std::string& to,
                                                       std::size_t max_depth) const {
    if (find(from) == nullptr || find(to) == nullptr) return {};
    std::unordered_map<std::string, std::string> predecessor;
    std::unordered_map<std::string, std::size_t> depth{{from, 0}};
    std::deque<std::string> queue{from};
    while (!queue.empty()) {
        const auto current = queue.front();
        queue.pop_front();
        if (current == to) break;
        if (depth.at(current) == max_depth) continue;
        for (const auto& edge : edges_) {
            const auto& adjacent = edge.from == current ? edge.to : (edge.to == current ? edge.from : std::string{});
            if (adjacent.empty() || depth.contains(adjacent)) continue;
            predecessor.emplace(adjacent, current);
            depth.emplace(adjacent, depth.at(current) + 1);
            queue.push_back(adjacent);
        }
    }
    if (!depth.contains(to)) return {};
    std::vector<std::string> path;
    for (std::string current = to;; current = predecessor.at(current)) {
        path.push_back(current);
        if (current == from) break;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

const std::vector<EvidenceEntity>& EvidenceGraph::entities() const noexcept { return entities_; }
const std::vector<EvidenceEdge>& EvidenceGraph::edges() const noexcept { return edges_; }

void write_evidence_graph(const std::filesystem::path& path, const EvidenceGraph& graph) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) throw std::runtime_error("Unable to write evidence graph: " + path.string());

    output << "{\n  \"schema_version\": 1,\n  \"entities\": [";
    for (std::size_t index = 0; index < graph.entities().size(); ++index) {
        const auto& entity = graph.entities()[index];
        if (index != 0) output << ',';
        output << "\n    {\"id\":\"" << escape_json(entity.id) << "\",\"kind\":\""
               << entity_kind_name(entity.kind) << "\",\"display_name\":\""
               << escape_json(entity.display_name) << '\"';
        if (entity.source.has_value()) {
            const auto& source = *entity.source;
            output << ",\"source\":{\"file\":\"" << escape_json(source.file.generic_string())
                   << "\",\"line\":" << source.line << ",\"column\":" << source.column
                   << ",\"end_line\":" << source.end_line << ",\"end_column\":" << source.end_column << '}';
        }
        output << '}';
    }
    output << "\n  ],\n  \"edges\": [";
    for (std::size_t index = 0; index < graph.edges().size(); ++index) {
        const auto& edge = graph.edges()[index];
        if (index != 0) output << ',';
        output << "\n    {\"from\":\"" << escape_json(edge.from) << "\",\"relation\":\""
               << escape_json(edge.relation) << "\",\"to\":\"" << escape_json(edge.to) << "\"}";
    }
    output << "\n  ]\n}\n";
}

}  // namespace mfe::flow
