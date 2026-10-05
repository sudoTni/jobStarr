#include <QApplication>
#include <QIcon>
#include "MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("jobStarr"));
    app.setApplicationDisplayName(QStringLiteral("jobStarr"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icon/jobStarr.png")));

    jobstarr::MainWindow mainWindow;
    mainWindow.show();

    return app.exec();
}
