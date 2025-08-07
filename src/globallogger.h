#ifndef GLOBALLOGGER_H
#define GLOBALLOGGER_H

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QThreadStorage>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>
#include <QApplication>
#include <QThread>
#include <QElapsedTimer>

enum LogLevel
{
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR_LEVEL,
    CRITICAL,
    FATAL
};

class GlobalLogger : public QObject
{
    Q_OBJECT

public:
    static GlobalLogger* instance();
    static void initialize(const QString& appName = "NetWire");
    
    void log(LogLevel level, const QString& message, const QString& function = "", const QString& file = "", int line = 0);
    void trace(const QString& message, const QString& function = "", const QString& file = "", int line = 0);
    void debug(const QString& message, const QString& function = "", const QString& file = "", int line = 0);
    void info(const QString& message, const QString& function = "", const QString& file = "", int line = 0);
    void warning(const QString& message, const QString& function = "", const QString& file = "", int line = 0);
    void error(const QString& message, const QString& function = "", const QString& file = "", int line = 0);
    void critical(const QString& message, const QString& function = "", const QString& file = "", int line = 0);
    void fatal(const QString& message, const QString& function = "", const QString& file = "", int line = 0);
    
    void setLogLevel(LogLevel level);
    void setLogToFile(bool enabled);
    void setLogToConsole(bool enabled);
    void setLogDirectory(const QString& directory);
    void setMaxLogFileSize(qint64 sizeInBytes);
    void setMaxLogFiles(int count);
    
    QString getLogDirectory() const;
    QString getCurrentLogFile() const;
    void rotateLogFiles();
    void clearOldLogs();
    
    void setThreadName(const QString& name);
    QString getThreadName() const;

private:
    GlobalLogger();
    ~GlobalLogger();
    
    void writeToFile(const QString& message);
    void writeToConsole(const QString& message);
    QString formatMessage(LogLevel level, const QString& message, const QString& function, const QString& file, int line);
    QString levelToString(LogLevel level);
    QString getTimestamp();
    void checkLogRotation();
    void logSystemInfo();
    
    static GlobalLogger* m_instance;
    static QMutex m_mutex;
    
    QString m_appName;
    QString m_logDirectory;
    QString m_currentLogFile;
    QFile* m_logFile;
    QTextStream* m_logStream;
    QMutex m_writeMutex;
    
    LogLevel m_logLevel;
    bool m_logToFile;
    bool m_logToConsole;
    qint64 m_maxLogFileSize;
    int m_maxLogFiles;
    
    QThreadStorage<QString> m_threadNames;
};

#define LOG_TRACE(msg) GlobalLogger::instance()->trace(msg, Q_FUNC_INFO, __FILE__, __LINE__)
#define LOG_DEBUG(msg) GlobalLogger::instance()->debug(msg, Q_FUNC_INFO, __FILE__, __LINE__)
#define LOG_INFO(msg) GlobalLogger::instance()->info(msg, Q_FUNC_INFO, __FILE__, __LINE__)
#define LOG_WARNING(msg) GlobalLogger::instance()->warning(msg, Q_FUNC_INFO, __FILE__, __LINE__)
#define LOG_ERROR(msg) GlobalLogger::instance()->error(msg, Q_FUNC_INFO, __FILE__, __LINE__)
#define LOG_CRITICAL(msg) GlobalLogger::instance()->critical(msg, Q_FUNC_INFO, __FILE__, __LINE__)
#define LOG_FATAL(msg) GlobalLogger::instance()->fatal(msg, Q_FUNC_INFO, __FILE__, __LINE__)

#define LOG_FUNCTION_ENTRY() LOG_DEBUG(QString("ENTER: %1").arg(Q_FUNC_INFO))
#define LOG_FUNCTION_EXIT() LOG_DEBUG(QString("EXIT: %1").arg(Q_FUNC_INFO))

#define LOG_VAR(var) LOG_DEBUG(QString("%1 = %2").arg(#var).arg(var))
#define LOG_EXCEPTION(e) LOG_ERROR(QString("Exception: %1").arg(e.what()))

#define DEBUG_FUNC() LOG_FUNCTION_ENTRY()
#define DEBUG_FUNC_EXIT() LOG_DEBUG("Function exit")
#define DEBUG_LINE() LOG_DEBUG(QString("Line %1").arg(__LINE__))
#define DEBUG_MSG(msg) LOG_DEBUG(msg)
#define DEBUG_VAR(var) LOG_VAR(var)
#define DEBUG_WRAP_FUNCTION() LOG_FUNCTION_ENTRY()

#endif // GLOBALLOGGER_H
