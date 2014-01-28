#include <Nubuck\polymesh.h>
#include <math\intersections.h>
#include <world\world.h>
#include <world\entities\ent_geometry\ent_geometry.h>
#include <renderer\mesh\mesh.h>
#include "op_translate.h"

namespace OP {

struct AxisMesh {
    std::vector<R::Mesh::Vertex>    vertices;
    std::vector<R::Mesh::Index>     indices;

    AxisMesh(
        float size, 
        int subdiv, 
        float spacing,
        const R::Color& colX, 
        const R::Color& colY, 
        const R::Color& colZ) 
    {
        unsigned idxCnt = 0;
        R::Color colors[] = { colX, colY, colZ };

        unsigned N = (1 << subdiv);
        float f = size / N;
        for(int i = 0; i < 3; ++i) {
            M::Vector3 d = M::Vector3::Zero;
            d.vec[i] = 1.0f;

            for(int j = 0; j < N; ++j) {
                R::Mesh::Vertex vert;
                vert.position = f * d * j;
                vert.color = colors[i];
                vertices.push_back(vert);
                indices.push_back(idxCnt++);

                M::Vector3 p = f * d * (j + 1) - spacing * f * d;
                vert.position = p;
                vert.color = colors[i];
                vertices.push_back(vert);
                indices.push_back(idxCnt++);
            }
        }
    }

    R::Mesh::Desc GetDesc() {
        R::Mesh::Desc desc;
        desc.vertices = &vertices[0];
        desc.numVertices = vertices.size();
        desc.indices = &indices[0];
        desc.numIndices = indices.size();
        desc.primType = GL_LINES;
        return desc;
    }
};

Translate::Translate() : _dragging(false) {
    _arrowHeadSize = scalar_t(1) / scalar_t(8);
    float off = _arrowHeadSize.to_float() + 0.8f;
    _arrowHeadsOffsets[X] = M::Vector3(off, 0.0f, 0.0f);
    _arrowHeadsOffsets[Y] = M::Vector3(0.0f, off, 0.0f);
    _arrowHeadsOffsets[Z] = M::Vector3(0.0f, 0.0f, off);

    _cursorPos = M::Vector3::Zero;

    AxisMesh axisDesc(1.0f, 4, 0.4f, R::Color::Red, R::Color::Blue, R::Color::Green);
    _axisMesh = R::meshMgr.Create(axisDesc.GetDesc());
    _axisTFMesh = R::meshMgr.Create(_axisMesh);
}

void Translate::BuildArrowHead() {
    leda::nb::RatPolyMesh mesh;
    leda::list<point3_t> L;
    const scalar_t& s = _arrowHeadSize;
    L.push(point3_t( 0,  2 * s,  0));
    L.push(point3_t(-s, -s, -s));
    L.push(point3_t(-s, -s,  s));
    L.push(point3_t( s, -s,  s));
    L.push(point3_t( s, -s, -s));
    leda::CONVEX_HULL(L, mesh);
    mesh.compute_faces();

    for(int i = 0; i < 3; ++i) {
        _geom_arrowHeads[i] = _nb.world->CreateGeometry();
        _geom_arrowHeads[i]->GetRatPolyMesh() = mesh;
        _geom_arrowHeads[i]->GetRatPolyMesh().compute_faces();
        _geom_arrowHeads[i]->SetRenderMode(IGeometry::RenderMode::FACES);
        _geom_arrowHeads[i]->SetRenderLayer(1);

        const M::Vector3& pos = _arrowHeadsOffsets[i];
        _geom_arrowHeads[i]->SetPosition(pos.x, pos.y, pos.z);
    }

    leda::nb::set_color(_geom_arrowHeads[X]->GetRatPolyMesh(), R::Color::Red);
    _geom_arrowHeads[X]->Rotate(-90.0f, 0.0f, 0.0f, 1.0f);

    leda::nb::set_color(_geom_arrowHeads[Y]->GetRatPolyMesh(), R::Color::Blue);

    leda::nb::set_color(_geom_arrowHeads[Z]->GetRatPolyMesh(), R::Color::Green);
    _geom_arrowHeads[Z]->Rotate(90.0f, 1.0f, 0.0f, 0.0f);

    for(int i = 0; i < 3; ++i) _geom_arrowHeads[i]->Update();
}

void Translate::BuildBBoxes() {
    const float l = 1.2f;
    const float w = 0.4f;
    _bboxes[X] = M::Box::FromCenterSize(M::Vector3(0.5f, 0.0f, 0.0f), M::Vector3(l, w, w));
    _bboxes[Y] = M::Box::FromCenterSize(M::Vector3(0.0f, 0.5f, 0.0f), M::Vector3(w, l, w));
    _bboxes[Z] = M::Box::FromCenterSize(M::Vector3(0.0f, 0.0f, 0.5f), M::Vector3(w, w, l));
}

void Translate::BuildCursor() {
    BuildArrowHead();
    BuildBBoxes();
}

void Translate::HideCursor() {
    for(int i = 0; i < DIM; ++i)
        _geom_arrowHeads[i]->Hide();
}

void Translate::ShowCursor() {
    for(int i = 0; i < DIM; ++i)
        _geom_arrowHeads[i]->Show();
}

void Translate::AlignWithCamera() {
    M::Matrix4 eyeToWorld, worldToEye = W::world.GetModelView();
    bool r = true;
    r = M::TryInvert(worldToEye, eyeToWorld);
    assert(r);
    M::Vector3 eye = M::Transform(eyeToWorld, M::Vector3::Zero); // eye pos in world space
    const float c = 10.0f;
    M::Vector3 d = _cursorPos - eye;
    M::Matrix4 M = M::Mat4::Translate(-(d.Length() - c) * M::Normalize(d));
    W::ENT_Geometry* ent = NULL;
    SetPosition(M::Transform(M, _cursorPos));
}

void Translate::Register(const Nubuck& nb, Invoker& invoker) {
    _nb = nb;

    QAction* action = _nb.ui->GetSceneMenu()->addAction("Translate");
    QObject::connect(action, SIGNAL(triggered()), &invoker, SLOT(OnInvoke()));

    BuildCursor();

    if(!W::world.SelectedGeometry()) HideCursor();
}

static M::Vector3 Axis(int i) {
    if(0 == i) return M::Vector3(1.0f, 0.0f, 0.0f);
    if(1 == i) return M::Vector3(0.0f, 1.0f, 0.0f);
    if(2 == i) return M::Vector3(0.0f, 0.0f, 1.0f);
    assert(false && "Axis(int i): parameter out of range");
    return M::Vector3::Zero;
}

void Translate::Invoke() {
    _nb.ui->SetOperatorName("Translate");
}

void Translate::GetMeshJobs(std::vector<R::MeshJob>& meshJobs) {
    R::MeshJob meshJob;
    meshJob.fx = "UnlitThickLines";
    meshJob.layer = 1;
    meshJob.material = R::Material::White;
    meshJob.primType = 0;
    meshJob.tfmesh = _axisTFMesh;
    meshJobs.push_back(meshJob);
}

void Translate::OnGeometrySelected() {
    W::ENT_Geometry* geom = (W::ENT_Geometry*)W::world.SelectedGeometry();
    _cursorPos = geom->GetTransform().position;
    SetPosition(_cursorPos);

    AlignWithCamera();
    ShowCursor();
}

void Translate::OnCameraChanged() {
    if(NULL != W::world.SelectedGeometry()) // ie. cursor is visible
        AlignWithCamera();
}

static float tmp;

bool Translate::OnMouseDown(const M::Vector2& mouseCoords) {
    if(!_dragging && NULL != W::world.SelectedGeometry()) {
        M::Ray ray = W::world.PickingRay(mouseCoords);

        M::Matrix3 M = M::RotationOf(W::world.GetModelView());
        float det = M::Det(M);
        if(M::AlmostEqual(0.0f, det)) printf("OMGOMGOMG\n");
        M::Matrix3 invM = M::Inverse(M, det);

        M::Vector3 eyeZ = M::Transform(invM, M::Vector3(0.0f, 0.0f, 1.0f)); // z axis of eye space in world space
        printf("eyeZ = %f %f %f\n", eyeZ.x, eyeZ.y, eyeZ.z);

        for(int i = 0; i < DIM; ++i) {
            if(M::IS::Intersects(ray, _bboxes[i])) {
                _dragAxis = i;
                _dragPlane = M::Plane::FromPointSpan(_cursorPos, M::Cross(eyeZ, Axis(i)), Axis(i));
                printf("Cross: %f %f %f, Axis(i): %f %f %f\n",
                    M::Cross(eyeZ, Axis(i)).x,
                    M::Cross(eyeZ, Axis(i)).y,
                    M::Cross(eyeZ, Axis(i)).z,
                    Axis(i).x,
                    Axis(i).y,
                    Axis(i).z);
                M::IS::Info inf;
                bool is = M::IS::Intersects(ray, _dragPlane, &inf);
                assert(is);
                tmp = _cursorPos.vec[_dragAxis];
                _dragOrig = inf.where;
                _dragging = true;
            }
        }
        if(_dragging) {
            printf("N = %f %f %f\n", _dragPlane.n.x, _dragPlane.n.y, _dragPlane.n.z);
            return true;
        }
        return false;
    }
    return false;
}

bool Translate::OnMouseUp(const M::Vector2& mouseCoords) {
    if(_dragging) {
        _dragging = false;
        return true;
    }
    return false;
}

static inline M::Matrix4 SetZ(const M::Vector3& pos, float z) {
    M::Matrix4 m = M::Mat4::Identity();
    m.m22 = z / pos.z;
    return m;
}

bool Translate::OnMouseMove(const M::Vector2& mouseCoords) {
    R::Color arrowHeadColors[] = {
        R::Color::Red,
        R::Color::Green,
        R::Color::Blue
    };
    if(!_dragging) {
        M::Ray ray = W::world.PickingRay(mouseCoords);
        for(int i = 0; i < DIM; ++i) {
            if(M::IS::Intersects(ray, _bboxes[i])) {
                leda::nb::set_color(_geom_arrowHeads[i]->GetRatPolyMesh(), R::BlendAddRGB(arrowHeadColors[i], R::Color::White));
            } else leda::nb::set_color(_geom_arrowHeads[i]->GetRatPolyMesh(), arrowHeadColors[i]);
            _geom_arrowHeads[i]->Update();
        }
    }

    if(_dragging) {
        M::Ray ray = W::world.PickingRay(mouseCoords);
        M::IS::Info inf;
        bool is = M::IS::Intersects(ray, _dragPlane, &inf);
        assert(is);
        M::Vector3 p = inf.where;
        _cursorPos.vec[_dragAxis] = tmp + (p - _dragOrig).vec[_dragAxis];
        printf("orig = %f %f %f, p = %f %f %f\n",
            _dragOrig.x, _dragOrig.y, _dragOrig.z,
            p.x, p.y, p.z);
        printf("COORD = %f\n", _cursorPos.vec[_dragAxis]);
        SetPosition(_cursorPos);

        IGeometry* geom = W::world.SelectedGeometry();
        geom->SetPosition(_cursorPos.x, _cursorPos.y, _cursorPos.z);

        AlignWithCamera();
        return true;
    }
    return false;
}

} // namespace OP