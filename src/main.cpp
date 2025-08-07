#include "mainwindow.h"
#include "globallogger.h"
#include "alertmanager.h"
#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QStyleFactory>
#include <QDebug>
#include <QDateTime>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>

// Windows-specific includes
#ifdef Q_OS_WIN
#include <windows.h>
#endif

void cleanupApplication() {
    LOG_INFO("Cleaning up application resources...");
    
    // Stop AlertManager monitoring if it's running
    AlertManager *alertManager = AlertManager::instance();
    if (alertManager) {
        alertManager->stopMonitoring();
        LOG_INFO("AlertManager monitoring stopped.");
    }
    
    // Stop all timers and background threads
    // This will be handled by the MainWindow destructor and NetworkMonitor destructor
    // but we can add additional cleanup here if needed
    
    LOG_INFO("Application cleanup completed.");
}

bool loadStyleSheet(QApplication &app) {
    LOG_DEBUG("Loading stylesheet");
    
    // Load the style sheet
    QFile styleFile(":/resources/style.qss");
    if (!styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        LOG_ERROR("Could not open stylesheet file");
        return false;
    }
    
    QString styleSheet = QLatin1String(styleFile.readAll());
    app.setStyleSheet(styleSheet);
    LOG_INFO("Stylesheet loaded successfully");
    return true;
}

int main(int argc, char *argv[])
{
    // Initialize global logger first
    GlobalLogger::initialize("NetWire");
    LOG_INFO("=== NETWIRE APPLICATION START ===");
    
    LOG_INFO("Starting NetWire application...");
    
    QApplication app(argc, argv);
    LOG_INFO("QApplication created successfully");
    
    // Set application information
    QApplication::setApplicationName("NetWire");
    QApplication::setApplicationVersion("0.1.0");
    QApplication::setOrganizationName("NetWire");
    LOG_INFO("Application information set");
    
    // Connect cleanup function to application quit signal
    QObject::connect(&app, &QApplication::aboutToQuit, cleanupApplication);
    
    // Set application icon (commented out due to empty icon file)
    // app.setWindowIcon(QIcon(":/resources/icons/app.ico"));
    LOG_INFO("Application icon set (skipped due to empty icon file)");
    
    // Load and apply style sheet
    if (!loadStyleSheet(app)) {
        LOG_WARNING("Failed to load style sheet");
    }
    
    // Create and show main window
    LOG_INFO("Creating MainWindow...");
    try {
        MainWindow *mainWindow = new MainWindow();
        LOG_INFO("MainWindow created successfully");
        
        LOG_INFO("Showing MainWindow...");
        mainWindow->show();
        LOG_INFO("MainWindow shown successfully");
        
        LOG_INFO("Starting event loop...");
        int result = app.exec();
        LOG_INFO(QString("Application event loop ended with code: %1").arg(result));
        
        delete mainWindow;
        return result;
    } catch (const std::exception& e) {
        LOG_CRITICAL(QString("Exception in MainWindow creation: %1").arg(e.what()));
        return 1;
    } catch (...) {
        LOG_CRITICAL("Unknown exception in MainWindow creation");
        return 1;
    }
}
