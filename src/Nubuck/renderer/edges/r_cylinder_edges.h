#pragma once

#include <generic\uncopyable.h>
#include <renderer\renderer.h>
#include <renderer\mesh\meshmgr_fwd.h>
#include "r_edges.h"

namespace R {

class CylinderEdges : private GEN::Uncopyable, public EdgeRenderer {
private:
    struct FatEdge : Edge {
        M::Matrix4 Rt; // object to intertia space for untransformed edges
        explicit FatEdge(const Edge& e) : Edge(e) { }
    };

    std::vector<FatEdge>        _edges;
    std::vector<Mesh::Vertex>   _edgeBBoxVertices;
    meshPtr_t                   _mesh;

    void DestroyMesh();
    void RebuildVertices(const M::Matrix4& transform);
public:
    CylinderEdges() : _mesh(NULL) { }
    ~CylinderEdges();

    bool IsEmpty() const override { return _edges.empty(); }

    void Clear() override;
    void Push(const Edge& edge) override { _edges.push_back(FatEdge(edge)); }
    void Rebuild() override;

    void SetTransform(const M::Matrix4& transform, const M::Matrix4& modelView) override;

    MeshJob GetRenderJob() const override;
};

} // namespace R