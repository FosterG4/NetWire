#include "src/globallogger.h"
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "Starting simple logger test...";
    
    // Initialize global logger
    GlobalLogger::initialize("SimpleTest");
    
    qDebug() << "Logger initialized, testing basic functionality...";
    
    // Test basic logging
    GlobalLogger::instance()->info("Test info message");
    GlobalLogger::instance()->debug("Test debug message");
    GlobalLogger::instance()->warning("Test warning message");
    GlobalLogger::instance()->error("Test error message");
    
    qDebug() << "Simple logger test completed successfully!";
    
    return 0;
}
