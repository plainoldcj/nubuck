#pragma once

#include <Nubuck\generic\uncopyable.h>
#include <renderer\mesh\mesh.h>
#include "r_nodes.h"

namespace R {

class PointNodes : public Nodes, private GEN::Uncopyable {
private:
    std::vector<unsigned>           _inMap; // maps polymesh vertex IDs to corresponding indices in _nodes
    std::vector<Node>               _nodes;
    std::vector<Mesh::Vertex>       _vertices;
    std::vector<Mesh::Index>        _indices;
    meshPtr_t                       _mesh;
    tfmeshPtr_t                     _tfmesh;
    bool                            _needsRebuild;
    bool                            _isInvalid;

    void DestroyMesh();
public:
    PointNodes() : _mesh(NULL), _tfmesh(NULL), _needsRebuild(false), _isInvalid(false) { }

    bool IsEmpty() const override { return _nodes.empty(); }

	void Rebuild(const leda::nb::RatPolyMesh& mesh, const std::vector<M::Vector3>& fpos) override;
    void Update(const leda::nb::RatPolyMesh& mesh, const std::vector<M::Vector3>& fpos) override;

    void SetColor(leda::node pv, const Color& color) override;

    void Transform(const M::Matrix4& objToWorld) override;

    void BuildRenderMesh() override;
    void DestroyRenderMesh() override;

    R::MeshJob GetRenderJob() const override;
};

} // namespace R