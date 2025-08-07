#include "src/globallogger.h"
#include <QCoreApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Initialize global logger
    GlobalLogger::initialize("SimpleTest");
    
    qDebug() << "Simple logger test started";
    
    // Test basic logging
    GlobalLogger::instance()->info("Test info message");
    GlobalLogger::instance()->debug("Test debug message");
    GlobalLogger::instance()->warning("Test warning message");
    GlobalLogger::instance()->error("Test error message");
    
    qDebug() << "Simple logger test completed";
    
    return 0;
}
