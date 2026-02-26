#include <QApplication>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("SonoStega");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("SonoStega");

    MainWindow w;
    w.show();

    return app.exec();
}
