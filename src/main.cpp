#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("NXSampler"));
    QApplication::setApplicationName(QStringLiteral("NXSampler"));
    QApplication::setApplicationVersion(QStringLiteral(NXSAMPLER_VERSION)); // from CMakeLists.txt

    MainWindow window;
    window.show();
    return QApplication::exec();
}
