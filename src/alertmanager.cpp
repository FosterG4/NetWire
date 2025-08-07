#include "alertmanager.h"
#include "networkmonitor.h"
#include <QSettings>
#include <QDateTime>
#include <QDebug>
#include <QNetworkInterface>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>

// Initialize static member
AlertManager* AlertManager::m_instance = nullptr;

AlertManager::AlertManager(QObject *parent)
    : QObject(parent)
    , m_nextAlertId(1)
    , m_isMonitoring(false)
    , m_monitorTimer(new QTimer(this))
    , m_lastTotalUpload(0)
    , m_lastAlertTime(QDateTime())
{
    connect(m_monitorTimer, &QTimer::timeout, this, &AlertManager::checkConnectionSpikes);
    
    // Initialize with default values
    initializeDefaults();
    
    // Load saved alerts and configuration
    loadConfiguration();
}

AlertManager::~AlertManager()
{
    saveConfiguration();
}

AlertManager* AlertManager::instance()
{
    if (!m_instance) {
        m_instance = new AlertManager(qApp);
    }
    return m_instance;
}

void AlertManager::initializeDefaults()
{
    // Set default threshold values
    m_thresholdConfig.bandwidthThreshold = 1024; // 1 MB/s
    m_thresholdConfig.connectionSpikeWindow = 60; // 1 minute
    m_thresholdConfig.connectionSpikeCount = 50; // 50 connections
    m_thresholdConfig.dataExfiltrationKB = 10 * 1024; // 10 MB
    
    // Enable all alert types by default
    for (int i = 0; i <= CustomAlert; ++i) {
        m_enabledAlertTypes.insert(static_cast<AlertType>(i));
    }
}

void AlertManager::loadConfiguration()
{
    QSettings settings("NetWire", "NetWire");
    
    // Load threshold configuration
    settings.beginGroup("AlertThresholds");
    m_thresholdConfig.bandwidthThreshold = settings.value("bandwidthThreshold", 1024).toInt();
    m_thresholdConfig.connectionSpikeWindow = settings.value("connectionSpikeWindow", 60).toInt();
    m_thresholdConfig.connectionSpikeCount = settings.value("connectionSpikeCount", 50).toInt();
    m_thresholdConfig.dataExfiltrationKB = settings.value("dataExfiltrationKB", 10 * 1024).toInt();
    settings.endGroup();
    
    // Load enabled alert types
    settings.beginGroup("EnabledAlerts");
    m_enabledAlertTypes.clear();
    for (int i = 0; i <= CustomAlert; ++i) {
        AlertType type = static_cast<AlertType>(i);
        QString key = QString("AlertType_%1").arg(i);
        if (settings.value(key, true).toBool()) {
            m_enabledAlertTypes.insert(type);
        }
    }
    settings.endGroup();
    
    // Load known applications
    m_knownApplications = QSet<QString>(settings.value("KnownApplications").toStringList().begin(), settings.value("KnownApplications").toStringList().end());
}

void AlertManager::saveConfiguration()
{
    QSettings settings("NetWire", "NetWire");
    
    // Save threshold configuration
    settings.beginGroup("AlertThresholds");
    settings.setValue("bandwidthThreshold", m_thresholdConfig.bandwidthThreshold);
    settings.setValue("connectionSpikeWindow", m_thresholdConfig.connectionSpikeWindow);
    settings.setValue("connectionSpikeCount", m_thresholdConfig.connectionSpikeCount);
    settings.setValue("dataExfiltrationKB", m_thresholdConfig.dataExfiltrationKB);
    settings.endGroup();
    
    // Save enabled alert types
    settings.beginGroup("EnabledAlerts");
    for (int i = 0; i <= static_cast<int>(AlertManager::CustomAlert); ++i) {
        AlertManager::AlertType type = static_cast<AlertManager::AlertType>(i);
        QString key = QString("AlertType_%1").arg(i);
        settings.setValue(key, m_enabledAlertTypes.contains(type));
    }
    settings.endGroup();
    
    // Save known applications
    QStringList knownAppsList;
    for (const QString &app : m_knownApplications) {
        knownAppsList.append(app);
    }
    settings.setValue("KnownApplications", knownAppsList);
}

void AlertManager::startMonitoring()
{
    if (m_isMonitoring) return;
    
    m_isMonitoring = true;
    m_monitorTimer->start(1000); // Check every second
    emit monitoringStateChanged(true);
}

void AlertManager::stopMonitoring()
{
    if (!m_isMonitoring) return;
    
    m_monitorTimer->stop();
    m_isMonitoring = false;
    emit monitoringStateChanged(false);
}

void AlertManager::checkNetworkActivity(const NetworkMonitor::NetworkStats &stats)
{
    if (!m_isMonitoring) return;
    
    // Check for new applications
    // This will be handled separately when we get application stats from NetworkMonitor
    
    // Check bandwidth usage
    checkBandwidthUsage(stats.downloadRate, stats.uploadRate);
    
    // Update recent bandwidth samples for moving average
    m_recentBandwidthSamples.append((stats.downloadRate + stats.uploadRate) / 1024); // Convert to KB/s
    while (m_recentBandwidthSamples.size() > BANDWIDTH_SAMPLE_COUNT) {
        m_recentBandwidthSamples.removeFirst();
    }
    
    // Check for data exfiltration
    if (m_lastTotalUpload > 0 && stats.totalUploaded > m_lastTotalUpload) {
        qint64 diff = stats.totalUploaded - m_lastTotalUpload;
        if (diff > static_cast<qint64>(m_thresholdConfig.dataExfiltrationKB) * 1024) {
            checkDataExfiltration(diff);
        }
    }
    m_lastTotalUpload = stats.totalUploaded;
}

void AlertManager::processNewConnection(const NetworkMonitor::ConnectionInfo &conn)
{
    if (!m_isMonitoring) return;
    
    // Update connection count for this source IP
    QString sourceKey = QString("%1:%2").arg(conn.localAddress).arg(conn.localPort);
    m_connectionCounts[sourceKey]++;
    
    // Check for suspicious connections
    checkSuspiciousConnections(conn);
}

void AlertManager::processTrafficData(quint64 download, quint64 upload)
{
    checkBandwidthUsage(download, upload);
}

void AlertManager::checkNewApplications(const QMap<QString, NetworkMonitor::NetworkStats> &appStats)
{
    if (!m_enabledAlertTypes.contains(NewAppDetected)) return;
    
    for (auto it = appStats.constBegin(); it != appStats.constEnd(); ++it) {
        const QString &appName = it.key();
        if (!m_knownApplications.contains(appName)) {
            // New application detected
            m_knownApplications.insert(appName);
            
            // Generate alert
            generateAlert(
                NewAppDetected,
                Medium,
                tr("New Application Detected"),
                tr("A new application has been detected on the network: %1").arg(appName),
                appName,
                "",
                0,
                ""
            );
        }
    }
}

void AlertManager::checkBandwidthUsage(quint64 download, quint64 upload)
{
    if (!m_enabledAlertTypes.contains(HighBandwidthUsage)) return;
    
    // Calculate total bandwidth in KB/s
    quint64 totalKbps = (download + upload) / 1024;
    
    // Check against threshold
    if (totalKbps > static_cast<quint64>(m_thresholdConfig.bandwidthThreshold)) {
        // Calculate moving average to avoid false positives on brief spikes
        qreal movingAvg = 0;
        for (quint64 sample : m_recentBandwidthSamples) {
            movingAvg += sample;
        }
        movingAvg = m_recentBandwidthSamples.isEmpty() ? 0 : movingAvg / m_recentBandwidthSamples.size();
        
        // Only alert if significantly above moving average
        if (totalKbps > movingAvg * 1.5) {
            generateAlert(
                HighBandwidthUsage,
                High,
                tr("High Bandwidth Usage Detected"),
                tr("Unusually high network bandwidth usage detected: %1 KB/s").arg(totalKbps),
                "",
                "",
                download + upload
            );
        }
    }
}

void AlertManager::checkSuspiciousConnections(const NetworkMonitor::ConnectionInfo &conn)
{
    if (!m_enabledAlertTypes.contains(SuspiciousConnection)) return;
    
    bool isSuspicious = false;
    QString reason;
    
    // Check for suspicious ports
    if (isSuspiciousPort(conn.remotePort)) {
        isSuspicious = true;
        reason = tr("Connecting to known suspicious port %1").arg(conn.remotePort);
    }
    
    // Check for suspicious IP addresses
    if (isSuspiciousIP(conn.remoteAddress)) {
        isSuspicious = true;
        reason = tr("Connecting to known suspicious IP address %1").arg(conn.remoteAddress);
    }
    
    // Check for unusual protocols
    if (conn.protocol != QAbstractSocket::TcpSocket && conn.protocol != QAbstractSocket::UdpSocket) {
        isSuspicious = true;
        reason = tr("Unusual network protocol detected: %1").arg(conn.protocol);
    }
    
    if (isSuspicious) {
        generateAlert(
            SuspiciousConnection,
            High,
            tr("Suspicious Connection Detected"),
            reason,
            conn.localAddress,
            conn.remoteAddress,
            0,
            QString("Process: %1, Protocol: %2").arg(conn.processName).arg(conn.protocol)
        );
    }
}

void AlertManager::checkConnectionSpikes()
{
    if (!m_enabledAlertTypes.contains(ConnectionSpike)) return;
    
    QDateTime now = QDateTime::currentDateTime();
    
    // Check for connection spikes
    for (auto it = m_connectionCounts.begin(); it != m_connectionCounts.end();) {
        if (it.value() > m_thresholdConfig.connectionSpikeCount) {
            // Generate alert for connection spike
            generateAlert(
                ConnectionSpike,
                High,
                tr("Connection Spike Detected"),
                tr("Unusually high number of connections detected: %1 connections in the last minute").arg(it.value()),
                it.key(),
                "",
                0
            );
            
            // Remove the entry to avoid duplicate alerts
            it = m_connectionCounts.erase(it);
        } else {
            ++it;
        }
    }
    
    // Clear old connection counts
    m_connectionCounts.clear();
}

void AlertManager::checkDataExfiltration(quint64 uploadSize)
{
    if (!m_enabledAlertTypes.contains(DataExfiltration)) return;
    
    // Check if the upload size exceeds the threshold
    if (uploadSize > static_cast<quint64>(m_thresholdConfig.dataExfiltrationKB) * 1024) {
        generateAlert(
            DataExfiltration,
            Critical,
            tr("Possible Data Exfiltration Detected"),
            tr("A large amount of data was uploaded: %1").arg(formatBytes(uploadSize)),
            "",
            "",
            uploadSize
        );
    }
}

void AlertManager::generateAlert(AlertType type, Severity severity, const QString &title,
                               const QString &description, const QString &source,
                               const QString &destination, qint64 bytes,
                               const QString &additionalInfo)
{
    AlertManager::Alert alert;
    alert.type = type;
    alert.severity = severity;
    alert.title = title;
    alert.description = description;
    alert.source = source;
    alert.destination = destination;
    alert.bytesTransferred = bytes;
    alert.timestamp = QDateTime::currentDateTime();
    alert.acknowledged = false;
    alert.additionalInfo = additionalInfo;
    
    // Add to active alerts and history
    int alertId = m_nextAlertId++;
    m_activeAlerts.insert(alertId, alert);
    m_alertHistory.append(alert);
    
    // Limit history size
    while (m_alertHistory.size() > MAX_ALERT_HISTORY) {
        m_alertHistory.removeFirst();
    }
    
    m_lastAlertTime = alert.timestamp;
    
    // Emit signals
    emit newAlert(alert);
    emit alertThresholdReached(type, title + ": " + description);
}

QList<AlertManager::Alert> AlertManager::activeAlerts() const
{
    return m_activeAlerts.values();
}

QList<AlertManager::Alert> AlertManager::alertHistory() const
{
    return m_alertHistory;
}

void AlertManager::acknowledgeAlert(int alertId)
{
    if (m_activeAlerts.contains(alertId)) {
        m_activeAlerts[alertId].acknowledged = true;
        emit alertAcknowledged(alertId);
    }
}

void AlertManager::clearAlert(int alertId)
{
    // Remove the alert and check if it existed
    int removedCount = m_activeAlerts.remove(alertId);
    if (removedCount > 0) {
        emit alertCleared(alertId);
    }
}

void AlertManager::clearAllAlerts()
{
    QList<int> ids = m_activeAlerts.keys();
    for (int id : ids) {
        clearAlert(id);
    }
}

AlertManager::ThresholdConfig AlertManager::thresholdConfig() const
{
    return m_thresholdConfig;
}

void AlertManager::setThresholdConfig(const ThresholdConfig &config)
{
    m_thresholdConfig = config;
    saveConfiguration();
}

bool AlertManager::isAlertTypeEnabled(AlertType type) const
{
    return m_enabledAlertTypes.contains(type);
}

void AlertManager::setAlertTypeEnabled(AlertType type, bool enabled)
{
    if (enabled) {
        m_enabledAlertTypes.insert(type);
    } else {
        m_enabledAlertTypes.remove(type);
    }
    saveConfiguration();
}

QString AlertManager::alertTypeToString(AlertType type) const
{
    switch (type) {
    case NewAppDetected: return tr("New Application");
    case HighBandwidthUsage: return tr("High Bandwidth Usage");
    case SuspiciousConnection: return tr("Suspicious Connection");
    case PortScanDetected: return tr("Port Scan Detected");
    case DataExfiltration: return tr("Data Exfiltration");
    case ProtocolAnomaly: return tr("Protocol Anomaly");
    case ConnectionSpike: return tr("Connection Spike");
    case RuleViolation: return tr("Rule Violation");
    case CustomAlert: return tr("Custom Alert");
    default: return tr("Unknown Alert Type");
    }
}

QString AlertManager::severityToString(Severity severity) const
{
    switch (severity) {
    case Info: return tr("Info");
    case Low: return tr("Low");
    case Medium: return tr("Medium");
    case High: return tr("High");
    case Critical: return tr("Critical");
    default: return tr("Unknown");
    }
}

bool AlertManager::isSuspiciousPort(quint16 port) const
{
    // Common suspicious ports (this is a simplified list)
    static const QSet<quint16> suspiciousPorts = {
        22,     // SSH (common target)
        23,     // Telnet (insecure)
        80,     // HTTP (common target)
        443,    // HTTPS (common target)
        445,    // SMB (often targeted)
        1433,   // MS SQL Server
        3306,   // MySQL
        3389,   // RDP
        5900,   // VNC
        8080    // Alternative HTTP
    };
    
    return suspiciousPorts.contains(port);
}

bool AlertManager::isSuspiciousIP(const QString &ip) const
{
    // This is a simplified check. In a real application, you might want to:
    // 1. Check against known malicious IP databases
    // 2. Look for IPs in suspicious ranges
    // 3. Check for connections to TOR exit nodes
    
    QHostAddress addr(ip);
    if (addr.isNull()) return false;
    
    // Check for localhost
    if (addr == QHostAddress::LocalHost || addr == QHostAddress::LocalHostIPv6) {
        return false;
    }
    
    // For now, we'll skip the subnet checks to avoid compatibility issues
    // In a production environment, you would want to implement proper IP range checking
    // or use a third-party library for this purpose
    
    return false;
}

bool AlertManager::isIPInRange(const QString &ip, const QString &range) const
{
    // Split the range into IP and prefix length
    QStringList parts = range.split('/');
    if (parts.size() != 2) {
        return false; // Invalid format
    }

    QString rangeIp = parts[0];
    bool ok;
    int prefix = parts[1].toInt(&ok);
    if (!ok || prefix < 0 || prefix > 32) {
        return false; // Invalid prefix length
    }

    // Convert IPs to quint32 for comparison
    QHostAddress ipAddr(ip);
    QHostAddress rangeAddr(rangeIp);
    
    if (ipAddr.isNull() || rangeAddr.isNull() || ipAddr.protocol() != QAbstractSocket::IPv4Protocol || 
        rangeAddr.protocol() != QAbstractSocket::IPv4Protocol) {
        return false; // Invalid IP or not IPv4
    }

    quint32 ipBits = ipAddr.toIPv4Address();
    quint32 rangeBits = rangeAddr.toIPv4Address();
    
    // Create a bitmask for the prefix
    quint32 mask = prefix == 0 ? 0 : ~((1 << (32 - prefix)) - 1);
    
    // Compare the network portions
    return (ipBits & mask) == (rangeBits & mask);
}

QString AlertManager::formatBytes(qint64 bytes) const
{
    const char *suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double size = bytes;
    
    while (size >= 1024 && i < 4) {
        size /= 1024;
        i++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(suffixes[i]);
}
