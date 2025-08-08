#ifndef INTRUSIONDETECTIONMANAGER_H
#define INTRUSIONDETECTIONMANAGER_H

#include <QObject>
#include <QHash>
#include <QTimer>
#include <QSet>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "networkmonitor.h"
#include "alertmanager.h"

class IntrusionDetectionManager : public QObject
{
    Q_OBJECT

public:
    // Threat levels
    enum ThreatLevel {
        None,       // No threat detected
        Low,        // Low-level threat
        Medium,     // Medium-level threat
        High,       // High-level threat
        Critical    // Critical threat
    };
    Q_ENUM(ThreatLevel)

    // Detection types
    enum DetectionType {
        SignatureBased,    // Pattern-based detection
        AnomalyBased,      // Behavioral anomaly detection
        HeuristicBased,    // Rule-based heuristic detection
        ThreatIntelBased   // Threat intelligence based
    };
    Q_ENUM(DetectionType)

    // Security event structure
    struct SecurityEvent {
        QString id;
        DetectionType type;
        ThreatLevel level;
        QString title;
        QString description;
        QString sourceIP;
        QString destinationIP;
        quint16 sourcePort;
        quint16 destinationPort;
        QString protocol;
        qint64 bytesTransferred;
        QDateTime timestamp;
        bool blocked;
        QString signature;
        QString additionalInfo;
    };

    // Threat intelligence data
    struct ThreatIntel {
        QString ipAddress;
        QString threatType;
        QString description;
        int confidence;
        QDateTime lastSeen;
        QString source;
    };

    // Detection signature
    struct DetectionSignature {
        QString id;
        QString name;
        QString pattern;
        QString description;
        ThreatLevel level;
        bool enabled;
        QString category;
    };

    static IntrusionDetectionManager* instance();
    ~IntrusionDetectionManager();

    // Threat management
    QList<SecurityEvent> recentEvents() const;
    QList<SecurityEvent> blockedEvents() const;
    ThreatLevel currentThreatLevel() const;
    int activeThreats() const;

    // Detection configuration
    QList<DetectionSignature> signatures() const;
    void addSignature(const DetectionSignature &signature);
    void removeSignature(const QString &signatureId);
    void enableSignature(const QString &signatureId, bool enabled = true);

    // Threat intelligence
    QList<ThreatIntel> threatIntelligence() const;
    void updateThreatIntelligence();
    bool isIPThreat(const QString &ipAddress) const;
    ThreatIntel getThreatInfo(const QString &ipAddress) const;

    // Privacy protection
    bool checkDNSLeak();
    bool checkVPNStatus();
    int calculatePrivacyScore() const;
    QStringList getPrivacyRecommendations() const;

    // Blocking actions
    void blockIP(const QString &ipAddress, const QString &reason = QString());
    void blockPort(quint16 port, const QString &reason = QString());
    void blockApplication(const QString &appPath, const QString &reason = QString());
    void unblockIP(const QString &ipAddress);
    void unblockPort(quint16 port);
    void unblockApplication(const QString &appPath);

public slots:
    void startMonitoring();
    void stopMonitoring();
    void processConnection(const NetworkMonitor::ConnectionInfo &conn);
    void processTrafficData(quint64 download, quint64 upload);
    void runSecurityScan();

signals:
    void securityEventDetected(const SecurityEvent &event);
    void threatLevelChanged(ThreatLevel level);
    void ipBlocked(const QString &ipAddress, const QString &reason);
    void ipUnblocked(const QString &ipAddress);
    void privacyScoreChanged(int score);
    void dnsLeakDetected();
    void vpnStatusChanged(bool connected);

private:
    explicit IntrusionDetectionManager(QObject *parent = nullptr);
    static IntrusionDetectionManager* m_instance;

    // Detection methods
    void performSignatureDetection(const NetworkMonitor::ConnectionInfo &conn);
    void performAnomalyDetection(const NetworkMonitor::ConnectionInfo &conn);
    void performHeuristicDetection(const NetworkMonitor::ConnectionInfo &conn);
    void checkThreatIntelligence(const QString &ipAddress);

    // Privacy protection methods
    void checkDNSConfiguration();
    void checkVPNConnection();
    void updatePrivacyScore();

    // Utility methods
    QString generateEventId() const;
    bool isKnownMaliciousIP(const QString &ipAddress) const;
    bool isSuspiciousPort(quint16 port) const;
    bool isSuspiciousProtocol(const QString &protocol) const;
    QString formatBytes(qint64 bytes) const;

    // Configuration
    void loadSignatures();
    void saveSignatures();
    void loadThreatIntelligence();
    void saveThreatIntelligence();
    void initializeDefaultSignatures();

    // Network access for threat intelligence
    void fetchThreatIntelligence();
    void onThreatIntelResponse(QNetworkReply *reply);

    // Member variables
    QList<SecurityEvent> m_recentEvents;
    QList<SecurityEvent> m_blockedEvents;
    QList<DetectionSignature> m_signatures;
    QList<ThreatIntel> m_threatIntelligence;
    QSet<QString> m_blockedIPs;
    QSet<quint16> m_blockedPorts;
    QSet<QString> m_blockedApplications;
    
    ThreatLevel m_currentThreatLevel;
    int m_privacyScore;
    bool m_dnsLeakDetected;
    bool m_vpnConnected;
    
    QTimer *m_scanTimer;
    QTimer *m_threatIntelTimer;
    QNetworkAccessManager *m_networkManager;
    
    QString m_signaturesFilePath;
    QString m_threatIntelFilePath;
};

#endif // INTRUSIONDETECTIONMANAGER_H
