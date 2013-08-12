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

QGLFormat FmtAlphaMultisampling(int numSamples) {
	QGLFormat fmt(QGL::AlphaChannel | QGL::SampleBuffers);

	fmt.setSampleBuffers(true);
	fmt.setSamples(numSamples);

	return fmt;
}

int RunNubuck(int argc, char* argv[], algAlloc_t algAlloc) {
    QGLFormat::setDefaultFormat(FmtAlphaMultisampling(4));
    QApplication app(argc, argv);

    COM::Config::Instance().DumpVariables();

    common.Init(argc, argv);
    R::CreateDefaultEffects();

#ifdef NUBUCK_MT
    W::world.Thread_StartAsync();
#endif

    nubuck.common   = &common;
    nubuck.world    = &W::world;
    nubuck.log      = UI::LogWidget::Instance();

    ALG::Driver::Instance().SetAlloc(algAlloc);

    UI::MainWindow mainWindow;
    mainWindow.show();
    return app.exec();
}