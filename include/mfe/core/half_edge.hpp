#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <map>
#include <utility>

namespace mfe {

struct Vertex {
    uint32_t id;
    double x;
    double y;
    int32_t halfedge = -1;       // ID of one outgoing half-edge
    double conformal_factor = 0.0; // Conformal factor lambda for Ricci flow
    mutable double curvature = 0.0; // Curvature (angle defect K_i)
};

struct HalfEdge {
    uint32_t id;
    int32_t vertex = -1;         // Source/start vertex ID
    int32_t target_vertex = -1;  // Target/end vertex ID
    int32_t opposite = -1;       // Twin/opposite half-edge ID (-1 if boundary)
    int32_t next = -1;           // Next half-edge ID in the face cycle
    int32_t face = -1;           // Face ID to the left (-1 if boundary/exterior)
    int32_t edge = -1;           // Associated edge ID
};

struct Edge {
    uint32_t id;
    int32_t halfedge = -1;       // One of the two half-edges representing this edge
    mutable double cotan_weight = 0.0; // Cotangent weight for DEC/Laplacian
};

struct Face {
    uint32_t id;
    int32_t halfedge = -1;       // One half-edge on this face
    mutable double area = 0.0;   // Face area
};

class HalfEdgeMesh {
public:
    std::vector<Vertex> vertices;
    std::vector<HalfEdge> halfedges;
    std::vector<Edge> edges;
    std::vector<Face> faces;

    // Helper functions for traversal
    int32_t next_halfedge(int32_t h) const { return halfedges[h].next; }
    int32_t opposite_halfedge(int32_t h) const { return halfedges[h].opposite; }
    int32_t target_vertex(int32_t h) const { return halfedges[h].target_vertex; }
    int32_t source_vertex(int32_t h) const { return halfedges[h].vertex; }
    
    bool is_boundary(int32_t h) const { return halfedges[h].face == -1; }

    // Build the mesh from positions and triangle indices
    bool build(const std::vector<std::pair<double, double>>& pts, const std::vector<std::array<uint32_t, 3>>& triangles) {
        vertices.clear();
        halfedges.clear();
        edges.clear();
        faces.clear();

        // 1. Create Vertices
        for (size_t i = 0; i < pts.size(); ++i) {
            Vertex v;
            v.id = static_cast<uint32_t>(i);
            v.x = pts[i].first;
            v.y = pts[i].second;
            v.halfedge = -1;
            vertices.push_back(v);
        }

        // Map to keep track of directed edges for opposite linkage: (u, v) -> halfedge_id
        std::map<std::pair<uint32_t, uint32_t>, int32_t> edge_map;

        // 2. Create Faces and Half-edges
        for (size_t f_idx = 0; f_idx < triangles.size(); ++f_idx) {
            const auto& tri = triangles[f_idx];
            
            Face f;
            f.id = static_cast<uint32_t>(f_idx);
            
            int32_t he_start_idx = static_cast<int32_t>(halfedges.size());
            f.halfedge = he_start_idx;
            faces.push_back(f);

            for (int i = 0; i < 3; ++i) {
                HalfEdge he;
                he.id = static_cast<uint32_t>(halfedges.size() + i);
                he.vertex = tri[i];
                he.target_vertex = tri[(i + 1) % 3];
                he.face = static_cast<int32_t>(f_idx);
                he.next = he_start_idx + ((i + 1) % 3);
                halfedges.push_back(he);

                // Set vertex outgoing halfedge if not set
                if (vertices[tri[i]].halfedge == -1) {
                    vertices[tri[i]].halfedge = he.id;
                }

                // Register in the map
                edge_map[{he.vertex, he.target_vertex}] = he.id;
            }
        }

        // 3. Link opposites and create Edges
        std::map<std::pair<uint32_t, uint32_t>, int32_t> unique_edges;
        for (auto& he : halfedges) {
            auto twin_it = edge_map.find({he.target_vertex, he.vertex});
            if (twin_it != edge_map.end()) {
                he.opposite = twin_it->second;
            } else {
                he.opposite = -1; // Boundary
            }

            // Create Edge objects
            uint32_t u = std::min(static_cast<uint32_t>(he.vertex), static_cast<uint32_t>(he.target_vertex));
            uint32_t v = std::max(static_cast<uint32_t>(he.vertex), static_cast<uint32_t>(he.target_vertex));
            auto edge_it = unique_edges.find({u, v});
            if (edge_it == unique_edges.end()) {
                Edge e;
                e.id = static_cast<uint32_t>(edges.size());
                e.halfedge = he.id;
                e.cotan_weight = 0.0;
                edges.push_back(e);
                unique_edges[{u, v}] = e.id;
                he.edge = e.id;
            } else {
                he.edge = edge_it->second;
            }
        }

        return true;
    }
};

} // namespace mfe
