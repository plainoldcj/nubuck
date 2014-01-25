#include <math\matrix3.h>
#include <renderer\mesh\meshmgr.h>
#include "r_nodes.h"

namespace R {

void Nodes::DestroyMesh() {
    if(_mesh) {
        meshMgr.Destroy(_mesh);
        _mesh = NULL;
    }
}

Nodes::~Nodes() {
    DestroyMesh();
}

void Nodes::Clear() {
    _nodes.clear();
    DestroyMesh();
}

// camera position at 0
static M::Matrix4 Billboard_FaceCamera(const M::Matrix4& worldMat, const M::Vector3& billboardPos) {
    const M::Vector3 p = M::Transform(worldMat, billboardPos);
    const M::Vector3 v2 = M::Normalize(-M::Vector3(p.x, 0.0f, p.z));
    const M::Vector3 v1 = M::Vector3(0.0f, 1.0f, 0.0f);
    const M::Vector3 v0 = M::Normalize(M::Cross(v1, v2));
    const M::Matrix3 M = M::Mat3::FromColumns(v0, v1, v2);
    const M::Vector3 w2 = M::Normalize(-M::Vector3(0.0f, p.y, p.z));
    const M::Vector3 w0 = M::Vector3(1.0f, 0.0f, 0.0f);
    const M::Vector3 w1 = M::Normalize(M::Cross(w0, w2));
    const M::Matrix3 N = M::Mat3::FromColumns(w0, w1, w2);
    return M::Mat4::FromRigidTransform(M * N, p);
}

// omits scale
static M::Matrix4 Billboard_FaceViewingPlane(const M::Matrix4& worldMat, const M::Vector3& billboardPos) {
    const M::Vector3 p = M::Transform(worldMat, billboardPos);
    return M::Matrix4(
        1.0f, 0.0f, 0.0f, worldMat.m03 + p.x,
        0.0f, 1.0f, 0.0f, worldMat.m13 + p.y,
        0.0f, 0.0f, 1.0f, worldMat.m23 + p.z,
        0.0f, 0.0f, 0.0f, 1.0f);
}

void Nodes::Rebuild() {
    unsigned numBillboards = _nodes.size();
    unsigned numBillboardIndices = 5 * numBillboards - 1;

    _billboards.clear();
    _billboards.resize(numBillboards);

    static const M::Vector2 bbTexCoords[4] = {
        M::Vector2(-1.0f, -1.0f),
        M::Vector2( 1.0f, -1.0f),
        M::Vector2( 1.0f,  1.0f),
        M::Vector2(-1.0f,  1.0f)
    };

    for(unsigned i = 0; i < numBillboards; ++i) {
        for(unsigned k = 0; k < 4; ++k) {
            _billboards[i].verts[k].color = _nodes[i].color;
            _billboards[i].verts[k].texCoords = bbTexCoords[k];
        }
    }

    _billboardIndices.clear();
    _billboardIndices.reserve(numBillboardIndices);
    for(unsigned i = 0; i < 4 * numBillboards; ++i) {
        if(0 < i && 0 == i % 4) _billboardIndices.push_back(Mesh::RESTART_INDEX);
        _billboardIndices.push_back(i);
    }
    assert(numBillboardIndices == _billboardIndices.size());


    if(!_nodes.empty()) {
        assert(NULL == _mesh);

        Mesh::Desc meshDesc;
        meshDesc.vertices = &_billboards[0].verts[0];
        meshDesc.numVertices = 4 * numBillboards;
        meshDesc.indices = &_billboardIndices[0];
        meshDesc.numIndices = numBillboardIndices;
        meshDesc.primType = GL_TRIANGLE_FAN;
        
        _mesh = meshMgr.Create(meshDesc);
    }
}

void Nodes::Transform(const M::Matrix4& modelView) {
    if(IsEmpty()) return;

	float nodeSize = cvar_r_nodeSize;
    const M::Vector3 bbPositions[4] = {
        M::Vector3(-nodeSize, -nodeSize, 0.0f),
        M::Vector3( nodeSize, -nodeSize, 0.0f),
        M::Vector3( nodeSize,  nodeSize, 0.0f),
        M::Vector3(-nodeSize,  nodeSize, 0.0f)
    };

    for(unsigned i = 0; i < _billboards.size(); ++i) {
        for(unsigned k = 0; k < 4; ++k) {
            _billboards[i].verts[k].position = M::Transform(modelView, _nodes[i].position) + bbPositions[k];
        }
    }

    if(_mesh) meshMgr.GetMesh(_mesh).Invalidate(&_billboards[0].verts[0]);
}

MeshJob Nodes::GetRenderJob() const {
    assert(!IsEmpty());

    R::MeshJob meshJob;
    meshJob.fx = "NodeBillboard";
    meshJob.mesh = _mesh;
    meshJob.material = Material::White;
    meshJob.primType = 0;
    meshJob.transform = M::Mat4::Identity();
    return meshJob;
}

} // namespace R