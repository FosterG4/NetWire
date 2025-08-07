#include "globallogger.h"
#include <QElapsedTimer>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QThread>
#include <QDebug>
#include <QTextStream>
#include <QMutexLocker>
#include <QTimer>
#include <QProcess>
#include <QOperatingSystemVersion>
#include <QSysInfo>
#include <QMutex>
#include <QThreadStorage>
#include <QElapsedTimer>

// Static member initialization
GlobalLogger* GlobalLogger::m_instance = nullptr;
QMutex GlobalLogger::m_mutex;

// ========================================
// GLOBAL LOGGER IMPLEMENTATION
// ========================================

GlobalLogger::GlobalLogger()
    : m_logFile(nullptr)
    , m_logStream(nullptr)
    , m_logLevel(LogLevel::DEBUG)
    , m_logToFile(true)
    , m_logToConsole(true)
    , m_maxLogFileSize(10 * 1024 * 1024) // 10MB
    , m_maxLogFiles(5)
{
    // Note: Can't use LOG_DEBUG here as logger isn't initialized yet
    qDebug() << "GlobalLogger constructor called";
}

GlobalLogger::~GlobalLogger()
{
    qDebug() << "GlobalLogger destructor called";
    
    if (m_logStream) {
        delete m_logStream;
        m_logStream = nullptr;
    }
    
    if (m_logFile) {
        if (m_logFile->isOpen()) {
            m_logFile->close();
        }
        delete m_logFile;
        m_logFile = nullptr;
    }
}

GlobalLogger* GlobalLogger::instance()
{
    if (!m_instance) {
        QMutexLocker locker(&m_mutex);
        if (!m_instance) {
            m_instance = new GlobalLogger();
        }
    }
    return m_instance;
}

void GlobalLogger::initialize(const QString& appName)
{
    qDebug() << "Initializing GlobalLogger for app:" << appName;
    
    // Get the instance first
    GlobalLogger* logger = instance();
    if (!logger) {
        qWarning() << "Failed to create GlobalLogger instance";
        return;
    }
    
    // Set the app name
    logger->m_appName = appName;
    
    // Set up log directory
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + 
                    "/NetWire/logs";
    
    QDir dir(logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    logger->m_logDirectory = logDir;
    
    // Create initial log file
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    logger->m_currentLogFile = QString("%1/%2_%3.log").arg(logger->m_logDirectory, logger->m_appName, timestamp);
    
    // Open log file
    logger->m_logFile = new QFile(logger->m_currentLogFile);
    if (logger->m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        logger->m_logStream = new QTextStream(logger->m_logFile);
        
        // Write header
        QString header = QString("=== %1 LOG STARTED AT %2 ===\n")
                        .arg(logger->m_appName)
                        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
        logger->writeToFile(header);
        
        // Log system information
        logger->logSystemInfo();
        
        logger->info("GlobalLogger initialized successfully");
        logger->info(QString("Log file: %1").arg(logger->m_currentLogFile));
        logger->info(QString("Log directory: %1").arg(logger->m_logDirectory));
    } else {
        qWarning() << "Failed to open log file:" << logger->m_currentLogFile;
        logger->m_logToFile = false;
    }
}

void GlobalLogger::log(LogLevel level, const QString& message, const QString& function, const QString& file, int line)
{
    if (level < m_logLevel) {
        return;
    }
    
    QString formattedMessage = formatMessage(level, message, function, file, line);
    
    if (m_logToFile) {
        writeToFile(formattedMessage);
    }
    
    if (m_logToConsole) {
        writeToConsole(formattedMessage);
    }
    
    // Check if we need to rotate log files
    checkLogRotation();
    
    // For fatal errors, exit the application
    if (level == LogLevel::FATAL) {
        critical("FATAL ERROR - Application will exit");
        QCoreApplication::quit();
    }
}

void GlobalLogger::trace(const QString& message, const QString& function, const QString& file, int line)
{
    log(LogLevel::TRACE, message, function, file, line);
}

void GlobalLogger::debug(const QString& message, const QString& function, const QString& file, int line)
{
    log(LogLevel::DEBUG, message, function, file, line);
}

void GlobalLogger::info(const QString& message, const QString& function, const QString& file, int line)
{
    log(LogLevel::INFO, message, function, file, line);
}

void GlobalLogger::warning(const QString& message, const QString& function, const QString& file, int line)
{
    log(LogLevel::WARNING, message, function, file, line);
}

void GlobalLogger::error(const QString& message, const QString& function, const QString& file, int line)
{
    log(LogLevel::ERROR, message, function, file, line);
}

void GlobalLogger::critical(const QString& message, const QString& function, const QString& file, int line)
{
    log(LogLevel::CRITICAL, message, function, file, line);
}

void GlobalLogger::fatal(const QString& message, const QString& function, const QString& file, int line)
{
    log(LogLevel::FATAL, message, function, file, line);
}

void GlobalLogger::setLogLevel(LogLevel level)
{
    m_logLevel = level;
    info(QString("Log level set to: %1").arg(levelToString(level)));
}

void GlobalLogger::setLogToFile(bool enabled)
{
    m_logToFile = enabled;
    info(QString("File logging %1").arg(enabled ? "enabled" : "disabled"));
}

void GlobalLogger::setLogToConsole(bool enabled)
{
    m_logToConsole = enabled;
    info(QString("Console logging %1").arg(enabled ? "enabled" : "disabled"));
}

void GlobalLogger::setLogDirectory(const QString& directory)
{
    m_logDirectory = directory;
    info(QString("Log directory set to: %1").arg(directory));
}

void GlobalLogger::setMaxLogFileSize(qint64 sizeInBytes)
{
    m_maxLogFileSize = sizeInBytes;
    info(QString("Max log file size set to: %1 bytes").arg(sizeInBytes));
}

void GlobalLogger::setMaxLogFiles(int count)
{
    m_maxLogFiles = count;
    info(QString("Max log files set to: %1").arg(count));
}

QString GlobalLogger::getLogDirectory() const
{
    return m_logDirectory;
}

QString GlobalLogger::getCurrentLogFile() const
{
    return m_currentLogFile;
}

void GlobalLogger::rotateLogFiles()
{
    info("Rotating log files");
    
    // Close current log file
    if (m_logStream) {
        delete m_logStream;
        m_logStream = nullptr;
    }
    
    if (m_logFile && m_logFile->isOpen()) {
        m_logFile->close();
    }
    
    // Create new log file
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    m_currentLogFile = QString("%1/%2_%3.log").arg(m_logDirectory, m_appName, timestamp);
    
    // Open new log file
    if (m_logFile) {
        delete m_logFile;
    }
    
    m_logFile = new QFile(m_currentLogFile);
    if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_logStream = new QTextStream(m_logFile);
        
        // Write header
        QString header = QString("=== %1 LOG ROTATED AT %2 ===\n")
                        .arg(m_appName)
                        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"));
        writeToFile(header);
        
        info("Log file rotated successfully");
        info(QString("New log file: %1").arg(m_currentLogFile));
    } else {
        qWarning() << "Failed to open new log file:" << m_currentLogFile;
        m_logToFile = false;
    }
    
    // Clean up old log files
    clearOldLogs();
}

void GlobalLogger::clearOldLogs()
{
    info("Cleaning up old log files");
    
    QDir dir(m_logDirectory);
    QStringList filters;
    filters << QString("%1_*.log").arg(m_appName);
    
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);
    
    // Keep only the most recent files
    while (files.size() > m_maxLogFiles) {
        QFileInfo oldestFile = files.takeLast();
        if (dir.remove(oldestFile.fileName())) {
            info(QString("Removed old log file: %1").arg(oldestFile.fileName()));
        } else {
            warning(QString("Failed to remove old log file: %1").arg(oldestFile.fileName()));
        }
    }
}

void GlobalLogger::setThreadName(const QString& name)
{
    m_threadNames.setLocalData(name);
}

QString GlobalLogger::getThreadName() const
{
    QString threadName = m_threadNames.localData();
    if (threadName.isEmpty()) {
        threadName = QString("Thread-%1").arg(QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId())));
    }
    return threadName;
}

void GlobalLogger::writeToFile(const QString& message)
{
    if (!m_logStream || !m_logFile || !m_logFile->isOpen()) {
        return;
    }
    
    QMutexLocker locker(&m_writeMutex);
    *m_logStream << message << Qt::endl;
    m_logStream->flush();
}

void GlobalLogger::writeToConsole(const QString& message)
{
    QMutexLocker locker(&m_writeMutex);
    qDebug().noquote() << message;
}

QString GlobalLogger::formatMessage(LogLevel level, const QString& message, const QString& function, const QString& file, int line)
{
    QString timestamp = getTimestamp();
    QString levelStr = levelToString(level);
    QString threadName = getThreadName();
    
    QString formattedMessage = QString("[%1] [%2] [%3] %4")
                              .arg(timestamp)
                              .arg(levelStr)
                              .arg(threadName)
                              .arg(message);
    
    // Add function, file, and line information if provided
    if (!function.isEmpty()) {
        formattedMessage += QString(" | Function: %1").arg(function);
    }
    
    if (!file.isEmpty() && line > 0) {
        QFileInfo fileInfo(file);
        formattedMessage += QString(" | File: %1:%2").arg(fileInfo.fileName()).arg(line);
    }
    
    return formattedMessage;
}

QString GlobalLogger::levelToString(LogLevel level)
{
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRIT";
        case LogLevel::FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

QString GlobalLogger::getTimestamp()
{
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
}

void GlobalLogger::checkLogRotation()
{
    if (!m_logFile || !m_logFile->isOpen()) {
        return;
    }
    
    if (m_logFile->size() >= m_maxLogFileSize) {
        info("Log file size limit reached, rotating logs");
        rotateLogFiles();
    }
}

void GlobalLogger::logSystemInfo()
{
    info("=== SYSTEM INFORMATION ===");
    info(QString("Application: %1").arg(m_appName));
    info(QString("Qt Version: %1").arg(QT_VERSION_STR));
    info(QString("Operating System: %1").arg(QOperatingSystemVersion::current().name()));
    info(QString("Architecture: %1").arg(QSysInfo::currentCpuArchitecture()));
    info(QString("Process ID: %1").arg(QString::number(QCoreApplication::applicationPid())));
    info(QString("Thread ID: %1").arg(QString::number(reinterpret_cast<quintptr>(QThread::currentThreadId()))));
    info(QString("Working Directory: %1").arg(QDir::currentPath()));
    info("=== END SYSTEM INFORMATION ===");
}
