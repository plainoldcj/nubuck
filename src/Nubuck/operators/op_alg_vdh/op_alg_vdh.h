#pragma once

#include <Nubuck\nb_common.h>
#include <Nubuck\editors\vertex_editor.h>

namespace OP {

enum {
    ID_DELAUNAY,
    ID_VORONOI
};

class VDH_Panel : public OperatorPanel {
private:
    NB::CheckBox _cbDelaunay;
    NB::CheckBox _cbVoronoi;
public:
    VDH_Panel();

    void Invoke() override;
};

class VDH_Operator : public Operator {
private:
    NB::Mesh _verticesMesh;
    NB::Mesh _delaunayMesh;
    NB::Mesh _voronoiMesh;

    leda::node_array<R::Color> _vertexColors;

    leda::GRAPH<leda::rat_point, int>   _voronoiTriang;
    leda::edge_map<leda::edge>          _voronoiMap;

    NB::VertexEditor _vertexEditor;

    void ApplyVoronoiColors();

    void Update();

    void Event_CheckBoxToggled(const EV::Arg<bool>& event);
public:
    VDH_Operator();

    void Register(Invoker& invoker) override;
    bool Invoke() override;
    void Finish() override;

    void OnMouse(const EV::MouseEvent& event) override;
};

} // namespace OP