#include <QApplication>
#include <QDebug>
#include "mainwindow_minimal.cpp"

int main(int argc, char *argv[])
{
    qDebug() << "Starting minimal NetWire test...";
    
    QApplication app(argc, argv);
    qDebug() << "QApplication created";
    
    try {
        qDebug() << "Creating MinimalMainWindow...";
        MinimalMainWindow window;
        qDebug() << "MinimalMainWindow created";
        
        qDebug() << "Showing window...";
        window.show();
        qDebug() << "Window shown";
        
        qDebug() << "Starting event loop...";
        int result = app.exec();
        qDebug() << "Event loop ended with code:" << result;
        
        return result;
    } catch (const std::exception& e) {
        qDebug() << "Exception:" << e.what();
        return 1;
    } catch (...) {
        qDebug() << "Unknown exception";
        return 1;
    }
}
