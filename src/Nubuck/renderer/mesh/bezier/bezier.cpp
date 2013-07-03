#include <assert.h>
#include "bezier.h"

namespace R {

    M::Vector2 DeCasteljau(const std::vector<M::Vector2>& points, int l, int r, float t) {
        assert(l <= r);
        if(l == r) return points[l];
        return (1.0f - t) * DeCasteljau(points, l, r - 1, t) + t * DeCasteljau(points, l + 1, r, t);
    }

    M::Vector2 Bezier::B(float t) {
        return DeCasteljau(_points, 0, _points.size() - 1,  t);
    }

    void Bezier::Build(void) {
        const float dt = 0.01f;
        Mesh::Vertex vert;
        vert.color = Color::Black;
        vert.normal = M::Vector3(0.0f, 0.0f, 1.0f);
        Mesh::Index idxCnt = 0;
        for(float t = 0.0f; t <= 1.0f; t += dt) {
            M::Vector2 p = B(t);
            vert.position = M::Vector3(p.x, p.y, 0.0f);
            _vertices.push_back(vert);
            _indices.push_back(idxCnt++);
        }
    }

    Bezier::Bezier(const std::vector<M::Vector2>& points) : _points(points) {
        Build();
    }

    Mesh::Desc Bezier::GetDesc(void) {
        Mesh::Desc desc;
        desc.vertices = &_vertices[0];
        desc.numVertices = _vertices.size();
        desc.indices = &_indices[0];
        desc.numIndices = _indices.size();
        desc.primType = GL_LINE_STRIP;
        return desc;
    }

} // namespace R