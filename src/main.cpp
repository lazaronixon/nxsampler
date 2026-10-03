#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("NXSampler"));
    QApplication::setApplicationName(QStringLiteral("NXSampler"));

    MainWindow window;
    window.show();
    return QApplication::exec();
}
