#pragma once

#include <QDialog>

#include "ui_nodeconfig.h"

namespace UI {

    class NodeConfig : public QDialog {
        Q_OBJECT
    private:
        Ui::NodeConfig _ui;
    private slots:
		void OnTypeChanged(int idx);
        void OnSizeChanged(double val);
        void OnSubdivisionsChanged(int val);
        void OnSmoothChanged(int val);
    public:
        NodeConfig(QWidget* parent = NULL); // Qt dictates public ctor

        static void Show(void) {
            static NodeConfig* instance = NULL;
            if(!instance) instance = new NodeConfig();
            instance->show();
        }
    };

} // namespace UI