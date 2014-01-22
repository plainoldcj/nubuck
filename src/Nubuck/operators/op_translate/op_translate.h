#pragma once

#include <vector>

#include <QMenu>

#include <Nubuck\nubuck.h>
#include <math\box.h>
#include <math\plane.h>
#include <operators\operator.h>
#include <operators\operators.h>
#include <renderer\mesh\mesh.h>

#include <LEDA\geo\d3_hull.h>

namespace OP {

class Translate : public Operator {
private:
    typedef leda::rational scalar_t;
    typedef leda::d3_rat_point point3_t;

    Nubuck _nb;

    IGeometry* _geom_axis;

    enum { X = 0, Y, Z, DIM };
    IGeometry*  _geom_arrowHeads[DIM];
    scalar_t    _arrowHeadSize;
    M::Vector3  _arrowHeadsOffsets[DIM];

    M::Box _bboxes[DIM];

    void BuildAxis();
    void BuildArrowHead();
    void BuildBBoxes();
    void BuildCursor();
    void ShowCursor();
    void HideCursor();

    void SetCenterPosition(M::Box& box, const M::Vector3& center) {
        const M::Vector3 oldCenter = 0.5f * (box.max - box.min) + box.min;
        const M::Vector3 d = (center - oldCenter);
        box.min += d;
        box.max += d;
    }

    void SetPosition(const M::Vector3& pos) {
        _geom_axis->SetPosition(pos.x, pos.y, pos.z);
        for(unsigned i = 0; i < DIM; ++i) {
            const M::Vector3& off = _arrowHeadsOffsets[i];
            _geom_arrowHeads[i]->SetPosition(off.x + pos.x, off.y + pos.y, off.z + pos.z);
            SetCenterPosition(_bboxes[i], pos);
    const float l = 1.2f;
    const float w = 0.2f;
    _bboxes[X] = M::Box::FromCenterSize(pos + M::Vector3(0.5f, 0.0f, 0.0f), M::Vector3(l, w, w));
    _bboxes[Y] = M::Box::FromCenterSize(pos + M::Vector3(0.0f, 0.5f, 0.0f), M::Vector3(w, l, w));
    _bboxes[Z] = M::Box::FromCenterSize(pos + M::Vector3(0.0f, 0.0f, 0.5f), M::Vector3(w, w, l));
        }
    }

    M::Vector3  _cursorPos;

    bool        _dragging;
    int         _dragAxis;
    M::Vector3  _dragOrig;
    M::Plane    _dragPlane;
public:
    Translate();

    void Register(const Nubuck& nb, Invoker& invoker) override;
    void Invoke() override;
    void Finish() override { }

    void OnGeometrySelected() override;
    bool OnMouseDown(const M::Vector2& mouseCoords) override;
    bool OnMouseUp(const M::Vector2& mouseCoords) override;
    bool OnMouseMove(const M::Vector2& mouseCoords) override;
};

} // namespace OP