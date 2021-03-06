#pragma once

#include <Nubuck\nubuck.h>
#include <Nubuck\polymesh.h>
#include <Nubuck\math\box.h>
#include <Nubuck\system\locks\spinlock.h>
#include <Nubuck\world\editmode.h>
#include <Nubuck\animation\animation.h>
#include <renderer\renderer.h>
#include <renderer\mesh\mesh.h>
#include <renderer\mesh\meshmgr.h>
#include <renderer\mesh\bezier\bezier.h>
#include <renderer\nodes\r_billboard_nodes.h>
#include <renderer\nodes\r_point_nodes.h>
#include <renderer\edges\r_cylinder_edges.h>
#include <renderer\edges\r_line_edges.h>
#include <renderer\edges\r_gl_line_edges.h>
#include <renderer\text\r_text.h>
#include <UI\outliner\outliner.h>
#include <world\entity.h>
#include "ent_geometry_events.h"

namespace W {

class ENT_Geometry : public Entity, public EV::EventHandler<> {
private:
    leda::nb::RatPolyMesh _ratPolyMesh;

	mutable SYS::SpinLock _mtx;

    UI::Outliner::itemHandle_t      _outlinerItem;

    R::BillboardNodes               _billboardNodes;
    R::PointNodes                   _pointNodes;
    R::Nodes*                       _nodeRenderer;
    R::CylinderEdges                _cylinderEdges;
    R::LineEdges                    _lineEdges;
    R::GL_LineEdges                 _glLineEdges;
    R::EdgeRenderer*                _edgeRenderer;

    R::Text                         _vertexLabels;
    bool                            _showVertexLabels;
    bool                            _xrayVertexLabels;
    float                           _vertexLabelSize;

    void RebuildRenderEdges();
    void RebuildVertexLabels();

    struct Face {
        unsigned idx;
        unsigned sz;
    };
    struct Curve {
        unsigned        faceIdx;
        float           time;
        R::Color        color;
        M::Matrix3      localToWorld; // face transformation. face's base vertex is local origin
        M::Vector3      origin;
        M::Vector3      normal;
        R::PolyBezier2U curve;
        bool            dirty;
    };
    std::vector<M::Vector3>         _fpos;
    std::vector<leda::node>         _vmap; // maps rendermesh vertex IDs to polymesh vertices
    std::vector<Face>               _faces;
    std::vector<Curve>              _curves;
    std::vector<R::Mesh::Vertex>    _vertices;
    std::vector<R::Mesh::Index>     _indices;
    R::Mesh::Desc                   _meshDesc;
    R::meshPtr_t                    _mesh;
    R::tfmeshPtr_t                  _tfmesh;
    bool                            _meshCompiled; // TODO: might be a race cond
    bool                            _forceRebuild;

    void ComputeCurveDecals(Curve& cv);
    void RebuildCurve(Curve& curve);

    void RebuildCurves();

    // the single mesh that stores all decals of all curves,
    // updated every frame
    struct DecalMesh {
        R::meshPtr_t                    m_mesh;
        R::tfmeshPtr_t                  m_tfmesh;

        std::vector<R::Mesh::Vertex>    m_vertices;
        std::vector<R::Mesh::Index>     m_indices;

        DecalMesh() : m_mesh(NULL), m_tfmesh(NULL) { }

        void Rebuild(const std::vector<Curve>& curves);
        void Render(R::RenderList& renderList);

    } _decalMesh;

    void ForceRebuild();

    void RebuildRenderMesh();
    void UpdateRenderMesh();
    void DestroyRenderMesh();

    float       _vertexScale;
    float       _edgeScale;
    R::Color    _edgeTint;

    typedef NB::ShadingMode ShadingMode;
    typedef NB::Pattern     Pattern;

    bool                _isHidden;
    int                 _renderMode;
    unsigned            _renderLayer;
    ShadingMode         _shadingMode;
    Pattern             _pattern;
    R::Color            _patternColor;
    bool                _stylizedHiddenLines;
    bool                _showWireframe;
    bool                _showNormals;
    R::RenderList       _renderList;

    float               _transparency;
    bool                _isTransparent;

    void ComputeCenter();

    // selection
    std::vector<leda::node> _vertexSelection;

    // list of attached animations
    A::Animation* _anims;

#pragma region EventHandlers
    void Event_ShowVertexLabels(const EV::Arg<bool>& event);
    void Event_XrayVertexLabels(const EV::Arg<bool>& event);
    void Event_SetVertexLabelSize(const EV::Arg<float>& event);
    void Event_VertexScaleChanged(const EV::Arg<float>& event);
    void Event_EdgeScaleChanged(const EV::Arg<float>& event);
    void Event_EdgeTintChanged(const EV::Arg<R::Color>& event);
    void Event_TransparencyChanged(const EV::Arg<float>& event);
    void Event_RenderModeChanged(const RenderModeEvent& event);
    void Event_EdgeShadingChanged(const EdgeShadingEvent& event);
#pragma endregion
public:
    ENT_Geometry();

    DECL_HANDLE_EVENTS(ENT_Geometry);

    bool IsDirty() const;

    void CacheFPos();

    struct VertexHit {
        leda::node  vert;
        float       dist;

        VertexHit() { }
        VertexHit(leda::node vert, float dist) : vert(vert), dist(dist) { }
    };

    bool TraceVertices(const M::Ray& ray, float radius, std::vector<VertexHit>& hits);

    void Select() { Entity::Select(); }

    // NOTE: these methods should only be called by ISelection
    void                    Select(const leda::node v);
    void                    ClearVertexSelection();

    std::vector<leda::node> GetVertexSelection() const;

    void SetTransparency(float transparency);

    bool IsMeshCompiled() const;
    void CompileMesh();

    const std::string& GetName() const { return Entity::GetName(); }

    void SetName(const std::string& name);

    M::Box GetBoundingBox() const override;

	void Destroy() { Entity::Destroy(); }
    void OnDestroy();

    UI::OutlinerView* CreateOutlinerView();

    leda::nb::RatPolyMesh&          GetRatPolyMesh();
    const leda::nb::RatPolyMesh&    GetRatPolyMesh() const;
    void Rebuild();

    void ApplyTransformation();

    float       GetVertexScale() const;
    float       GetEdgeScale() const;
    R::Color    GetEdgeTint() const;

    void SetVertexScale(float vertexScale);
    void SetEdgeScale(float edgeScale);
    void SetEdgeTint(const R::Color& color);

    M::Vector3 GetPosition() const;

    void SetPosition(const M::Vector3& position) override;
    void SetScale(const M::Vector3& scale);

    void HideOutline();

    void Show();
    void Hide();

    ShadingMode         GetShadingMode() const;
    bool                StylizedHiddenLinesEnabled() const;

    void SetRenderMode(int flags);
    void SetRenderLayer(unsigned layer);
    void SetShadingMode(ShadingMode mode);
    void SetPattern(Pattern pattern);
    void SetPatternColor(const R::Color& color);

    int GetRenderMode() const { return _renderMode; }

    bool    GetVertexLabelsVisible() const { return _showVertexLabels; }
    float   GetVertexLabelSize() const { return _vertexLabelSize; }
    bool    GetXrayVertexLabels() const { return _xrayVertexLabels; }

    void FrameUpdate(float secsPassed);
    void BuildRenderList();
    const R::RenderList& GetRenderList() const { return _renderList; }

    void AttachAnimation(A::Animation* animation);

    void AddCurve(leda::face face, const R::Color& color);
};

void SetColorsFromVertexSelection(
    leda::nb::RatPolyMesh& mesh,
    const leda::node_map<bool>& selection,
    const R::Color& col_unselected,
    const R::Color& col_selected);
void SetColorsFromVertexSelection(ENT_Geometry& geom);

void SaveGeometryToFile(const std::string& filename, ENT_Geometry* geom);
void LoadGeometryFromFile(const std::string& filename, ENT_Geometry* geom);

} // namespace W