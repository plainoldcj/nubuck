#pragma once

#include <QGridLayout>
#include <QSpinBox>
#include <QPushButton>
#include <QWidget>

#include <Nubuck\operators\operator.h>

namespace UI {

class SimplePanel : public OP::OperatorPanel {
private:
    QGridLayout* _grid;
public:
    SimplePanel(QWidget* parent = NULL);

    void            AddLabel(const QString& str);
    QSpinBox*   	AddSpinBox(const QString& str, int min, int max);
    QPushButton*    AddPushButton(const QString& str);
};

} // namespace UI