#ifndef ALERTMANAGER_H
#define ALERTMANAGER_H

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QSet>
#include <QDateTime>
#include "networkmonitor.h"

class AlertManager : public QObject
{
    Q_OBJECT

public:
    // Alert types
    enum AlertType {
        NewAppDetected,         // New application detected on the network
        HighBandwidthUsage,     // Unusually high bandwidth usage
        SuspiciousConnection,   // Connection to known suspicious IP/port
        PortScanDetected,       // Possible port scan detected
        DataExfiltration,       // Unusual outbound data transfer
        ProtocolAnomaly,        // Unusual protocol usage
        ConnectionSpike,        // Sudden increase in connection count
        RuleViolation,          // Firewall rule violation attempt
        CustomAlert             // User-defined alert
    };
    Q_ENUM(AlertType)

    // Alert severity levels
    enum Severity {
        Info,       // Informational message
        Low,        // Low severity issue
        Medium,     // Medium severity issue
        High,       // High severity issue
        Critical    // Critical issue
    };
    Q_ENUM(Severity)

    // Alert structure
    struct Alert {
        AlertType type;
        Severity severity;
        QString title;
        QString description;
        QString source;
        QString destination;
        qint64 bytesTransferred;
        QDateTime timestamp;
        bool acknowledged;
        QString additionalInfo;
    };

    // Threshold configuration
    struct ThresholdConfig {
        int bandwidthThreshold;      // KB/s for high bandwidth alert
        int connectionSpikeWindow;   // Seconds to monitor for connection spikes
        int connectionSpikeCount;    // Number of connections to trigger alert
        int dataExfiltrationKB;      // KB threshold for data exfiltration
    };

    static AlertManager* instance();
    ~AlertManager();

    // Alert management
    QList<Alert> activeAlerts() const;
    QList<Alert> alertHistory() const;
    void acknowledgeAlert(int alertId);
    void clearAlert(int alertId);
    void clearAllAlerts();

    // Configuration
    ThresholdConfig thresholdConfig() const;
    void setThresholdConfig(const ThresholdConfig &config);
    bool isAlertTypeEnabled(AlertType type) const;
    void setAlertTypeEnabled(AlertType type, bool enabled);

public slots:
    void startMonitoring();
    void stopMonitoring();
    void checkNetworkActivity(const NetworkMonitor::NetworkStats &stats);
    void processNewConnection(const NetworkMonitor::ConnectionInfo &conn);
    void processTrafficData(quint64 download, quint64 upload);

signals:
    void newAlert(const Alert &alert);
    void alertAcknowledged(int alertId);
    void alertCleared(int alertId);
    void alertThresholdReached(AlertType type, const QString &message);
    void monitoringStateChanged(bool isMonitoring);

private:
    explicit AlertManager(QObject *parent = nullptr);
    static AlertManager* m_instance;

    // Alert generation helpers
    void generateAlert(AlertType type, Severity severity, const QString &title,
                      const QString &description, const QString &source = QString(),
                      const QString &destination = QString(), qint64 bytes = 0,
                      const QString &additionalInfo = QString());
    
    // Detection methods
    void checkNewApplications(const NetworkMonitor::NetworkStats &stats);
    void checkBandwidthUsage(quint64 download, quint64 upload);
    void checkSuspiciousConnections(const NetworkMonitor::ConnectionInfo &conn);
    void checkConnectionSpikes();
    void checkDataExfiltration(quint64 upload);
    
    // Helper methods
    QString alertTypeToString(AlertType type) const;
    QString severityToString(Severity severity) const;
    bool isSuspiciousPort(quint16 port) const;
    bool isSuspiciousIP(const QString &ip) const;
    
    // Alert storage
    QHash<int, Alert> m_activeAlerts;
    QList<Alert> m_alertHistory;
    int m_nextAlertId;
    
    // Monitoring state
    bool m_isMonitoring;
    QTimer *m_monitorTimer;
    
    // Configuration
    ThresholdConfig m_thresholdConfig;
    QSet<AlertType> m_enabledAlertTypes;
    
    // Tracking state
    QSet<QString> m_knownApplications;
    QHash<QString, int> m_connectionCounts;
    QList<quint64> m_recentBandwidthSamples;
    QDateTime m_lastAlertTime;
    quint64 m_lastTotalUpload;
    
    // Constants
    static const int MAX_ALERT_HISTORY = 1000;
    static const int BANDWIDTH_SAMPLE_COUNT = 5;
    static const int CONNECTION_SPIKE_WINDOW = 60; // seconds
    
    // Initialize with default values
    void initializeDefaults();
};

#endif // ALERTMANAGER_H
