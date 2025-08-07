#include "src/globallogger.h"
#include <QCoreApplication>
#include <QTimer>
#include <QThread>
#include <QElapsedTimer>

void testLogging() {
    LOG_INFO("=== LOGGER TEST START ===");
    
    // Test different log levels
    LOG_TRACE("This is a trace message");
    LOG_DEBUG("This is a debug message");
    LOG_INFO("This is an info message");
    LOG_WARNING("This is a warning message");
    LOG_ERROR("This is an error message");
    LOG_CRITICAL("This is a critical message");
    
    // Test function entry/exit
    LOG_FUNCTION_ENTRY();
    LOG_DEBUG("Inside test function");
    
    // Test variable logging
    int testVar = 42;
    QString testString = "Hello World";
    LOG_VAR(testVar);
    LOG_VAR(testString);
    
    // Test performance logging
    LOG_PERFORMANCE_START(testOperation);
    QThread::msleep(100); // Simulate work
    LOG_PERFORMANCE_END(testOperation);
    
    // Test thread logging
    LOG_THREAD_START();
    LOG_DEBUG("Thread operation");
    LOG_THREAD_END();
    
    LOG_INFO("=== LOGGER TEST END ===");
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    // Initialize global logger
    GlobalLogger::initialize("LoggerTest");
    
    LOG_INFO("Logger test application started");
    
    // Test basic logging
    testLogging();
    
    // Test log rotation
    LOG_INFO("Testing log rotation...");
    GlobalLogger::instance()->setMaxLogFileSize(1024); // 1KB for testing
    for (int i = 0; i < 100; ++i) {
        LOG_INFO(QString("Log message %1 - This is a long message to test log rotation").arg(i));
    }
    
    LOG_INFO("Logger test completed successfully");
    
    return 0;
}
