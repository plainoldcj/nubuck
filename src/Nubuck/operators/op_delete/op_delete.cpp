#include <QMenu>
#include <world\world.h>
#include "op_delete.h"

namespace OP {

void Delete::Register(const Nubuck& nb, Invoker& invoker) {
    _nb = nb;

    QAction* action = _nb.ui->GetObjectMenu()->addAction("Delete");
    QObject::connect(action, SIGNAL(triggered()), &invoker, SLOT(OnInvoke()));
}

void Delete::Invoke() {
    _nb.ui->SetOperatorName("Delete");

    const std::vector<IGeometry*>& geomList = W::world.GetSelection().GetGeometryList();
    for(unsigned i = 0; i < geomList.size(); ++i) {
        geomList[i]->Destroy();
    }
    W::world.ClearSelection();
}

} // namespace OP