#pragma once

#include <Nubuck\operators\operator.h>
#include <operators\operators.h>

namespace OP {

class MergeVerticesPanel : public OperatorPanel {
public:
    MergeVerticesPanel(QWidget* parent = NULL) : OperatorPanel(parent) { }
};

class MergeVertices : public Operator {
public:
    void Register(const Nubuck& nb, Invoker& invoker) override;
    bool Invoke() override;
    void Finish() override { }
};

} // namespace OP