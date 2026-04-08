#include <QApplication>
#include "app/MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("NF0T Logger");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("NF0T");

    MainWindow window;
    window.show();

    return app.exec();
}
