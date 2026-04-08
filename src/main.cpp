#include <QApplication>
#include "app/MainWindow.h"
#include "app/settings/SecureSettings.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // These three lines determine where QSettings writes its INI file:
    //   Linux:   ~/.config/NF0T/NF0T Logger.ini
    //   macOS:   ~/Library/Preferences/NF0T.NF0T Logger.plist
    //   Windows: HKCU\Software\NF0T\NF0T Logger
    app.setApplicationName("NF0T Logger");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("NF0T");

    // Begin async keychain load immediately — will be ready long before
    // the user can interact with any credential-dependent feature.
    SecureSettings::instance().loadAll();

    MainWindow window;
    window.show();

    return app.exec();
}
