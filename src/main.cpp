#include <QApplication>
#include <QIcon>
#include "ui/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("SonoStega");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("SonoStega");
    app.setWindowIcon(QIcon(":/icon.svg"));

    MainWindow w;
    w.show();

    return app.exec();
}
