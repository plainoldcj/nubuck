#include <QtGui\QApplication.h>

#include <renderer\glew\glew.h>
#include <QGLWidget>

#include <Nubuck\nubuck.h>
#include <common\common.h>
#include <common\config\config.h>
#include <world\world.h>
#include <renderer\effects\effect.h>
#include <algdriver\algdriver.h>
#include <UI\mainwindow\mainwindow.h>
#include <UI\logwidget\logwidget.h>
#include "nubuck_private.h"

Nubuck nubuck;

namespace {

    std::string ReadFile(const std::string& filename) {
        std::string text;

        std::ifstream file(filename.c_str());
        if(file.is_open()) {
            std::string line;
            while(file.good()) {
                std::getline(file, line);
                text += line;
            }
        }

        return text;
    }

} // unnamed namespace

QGLFormat FmtAlphaMultisampling(int numSamples) {
	QGLFormat fmt(QGL::AlphaChannel | QGL::SampleBuffers);

    if(0 < numSamples) {
        fmt.setSampleBuffers(true);
        fmt.setSamples(numSamples);
    }

	return fmt;
}

int RunNubuck(int argc, char* argv[], algAlloc_t algAlloc) {
    QGLFormat::setDefaultFormat(FmtAlphaMultisampling(0));
    QApplication app(argc, argv);

    COM::Config::Instance().DumpVariables();

    common.Init(argc, argv);
    R::CreateDefaultEffects();

    unsigned i = 0;
    while(i < argc - 1) {
        if(!strcmp("--stylesheet", argv[i])) {
            std::string stylesheet = common.BaseDir() + argv[i + 1];
            common.printf("INFO - reading stylesheet: %s\n", stylesheet.c_str());
            QString styleSheet(QString::fromStdString(ReadFile(stylesheet.c_str())));
            app.setStyleSheet(styleSheet);
        }
        i++;
    }

#ifdef NUBUCK_MT
    W::world.Thread_StartAsync();
#endif

    nubuck.common   = &common;
    nubuck.world    = &W::world;
    nubuck.log      = UI::LogWidget::Instance();

    ALG::gs_algorithm.SetAlloc(algAlloc);

    UI::MainWindow mainWindow;
    mainWindow.show();
    return app.exec();
}