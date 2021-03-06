#pragma once

#include <Nubuck\nubuck.h>
#include "shared.h"

class Mantle {
private:
    const mesh_t&               _subj; // mesh to be wrapped by mantle
    NB::Mesh                    _geom;
    leda::node_map<leda::node>  _nmap; // subj nodes -> mantle nodes
    leda::edge                  _cand[2];
    leda::node                  _last;

    void Init(const leda::node v0, const leda::node v1, const leda::node v2);
public:
    Mantle(const mesh_t& subj);

    void AddTriangle(const leda::node v0, const leda::node v1, const leda::node v2);

    void Destroy();
};