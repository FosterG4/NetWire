#include "mainwindow.h"
#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QStyleFactory>
#include <QDebug>

bool loadStyleSheet(QApplication &app) {
    // Load the style sheet
    QFile styleFile(":/style.qss");
    if (!styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open stylesheet file";
        return false;
    }
    
    QString styleSheet = QLatin1String(styleFile.readAll());
    app.setStyleSheet(styleSheet);
    return true;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // Set application information
    QApplication::setApplicationName("NetWire");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("NetWire");
    
    // Set application icon
    app.setWindowIcon(QIcon(":/resources/icons/app.ico"));
    
    // Load and apply style sheet
    if (!loadStyleSheet(app)) {
        qWarning() << "Failed to load style sheet";
    }
    
    // Create and show main window
    MainWindow mainWindow;
    mainWindow.show();
    
    return app.exec();
}
