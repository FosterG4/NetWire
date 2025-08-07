#ifndef NETWORKMONITOR_H
#define NETWORKMONITOR_H

#include <QObject>
#include <QStringList>
#include <QMap>
#include <QHash>
#include <QProcess>
#include <QIcon>
#include <QMutex>
#include <QTimer>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QProcess>
#include <QFileIconProvider>
#include <QFuture>
#include <QFutureWatcher>
#include <QDateTime>
#include <QQueue>

#ifdef HAVE_PCAP
#include <pcap.h>
#endif

class NetworkMonitor : public QObject
{
    Q_OBJECT

public:
    struct NetworkStats {
        quint64 bytesReceived;
        quint64 bytesSent;
        quint64 packetsReceived;
        quint64 packetsSent;
        QString processName;
        QIcon processIcon;
        qint64 processId;
        quint64 downloadRate;
        quint64 uploadRate;
        quint64 downloadTotal;
        quint64 uploadTotal;
        quint64 totalDownloaded;
        quint64 totalUploaded;
        
        NetworkStats() : bytesReceived(0), bytesSent(0), 
                        packetsReceived(0), packetsSent(0), 
                        processId(-1), downloadRate(0), 
                        uploadRate(0), downloadTotal(0),
                        uploadTotal(0), totalDownloaded(0),
                        totalUploaded(0) {}
    };
    
    struct ConnectionInfo {
        QString localAddress;
        quint16 localPort;
        QString remoteAddress;
        quint16 remotePort;
        int protocol; // 6=TCP, 17=UDP
        qint64 processId;
        QString processName;
        QString processPath;
        QIcon processIcon;
        QDateTime connectionTime;
        QDateTime lastActivity;
        quint64 bytesReceived;
        quint64 bytesSent;
        QString connectionState; // ESTABLISHED, LISTENING, TIME_WAIT, etc.
        QString remoteHostname;
        QString serviceName;
        
        ConnectionInfo() : localPort(0), remotePort(0), protocol(0), 
                          processId(-1), bytesReceived(0), bytesSent(0) {}
    };
    
    struct ConnectionHistory {
        QString localAddress;
        quint16 localPort;
        QString remoteAddress;
        quint16 remotePort;
        int protocol;
        qint64 processId;
        QString processName;
        QDateTime startTime;
        QDateTime endTime;
        quint64 totalBytesReceived;
        quint64 totalBytesSent;
        QString terminationReason; // "Normal", "Timeout", "Error", "User"
        
        ConnectionHistory() : localPort(0), remotePort(0), protocol(0), 
                            processId(-1), totalBytesReceived(0), totalBytesSent(0) {}
    };
    
    struct ConnectionFilter {
        QString processName;
        QString localAddress;
        QString remoteAddress;
        int protocol; // -1 for all protocols
        QString connectionState;
        bool showActiveOnly;
        QDateTime startTime;
        QDateTime endTime;
        
        ConnectionFilter() : protocol(-1), showActiveOnly(true) {}
    };

    explicit NetworkMonitor(QObject *parent = nullptr);
    ~NetworkMonitor();

    bool initialize();
    QStringList getAvailableInterfaces() const;
    bool startCapture(const QString &interfaceName);
    void stopCapture();
    void updateNetworkStats();
    QMap<qint64, NetworkStats> getStats() const;
    QList<ConnectionInfo> getActiveConnections() const;
    QMap<QString, NetworkStats> getStatsByApplication() const;
    QString getApplicationPath(const QString &appName) const;
    NetworkStats getApplicationStats(const QString &appName) const;
    
    // Phase 2: Enhanced Connection Management
    QList<ConnectionInfo> getFilteredConnections(const ConnectionFilter &filter) const;
    QList<ConnectionHistory> getConnectionHistory(const ConnectionFilter &filter = ConnectionFilter()) const;
    ConnectionInfo getConnectionDetails(const QString &localAddr, quint16 localPort, 
                                     const QString &remoteAddr, quint16 remotePort, int protocol) const;
    bool terminateConnection(const QString &localAddr, quint16 localPort, 
                           const QString &remoteAddr, quint16 remotePort, int protocol);
    void exportConnectionData(const QString &filename, const ConnectionFilter &filter = ConnectionFilter());
    QMap<QString, quint64> getConnectionStatistics() const;
    
    // Phase 2: Application Profiling
    QMap<QString, NetworkStats> getApplicationProfiles() const;
    QList<QString> getSuspiciousApplications() const;
    QMap<QString, QList<ConnectionInfo>> getApplicationConnections() const;
    void setApplicationMonitoring(const QString &appName, bool enabled);
    bool isApplicationMonitored(const QString &appName) const;
    
    // Phase 2: Traffic Analysis
    QMap<QString, quint64> getProtocolStatistics() const;
    QMap<QString, quint64> getPortStatistics() const;
    QList<QString> getTopTalkers() const;
    QList<QString> getTopListeners() const;
    double getNetworkLatency(const QString &host) const;
    quint64 getPacketLossRate() const;

signals:
    void networkDataUpdated();
    void statsUpdated(quint64 download, quint64 upload);
    void connectionCountChanged(int count);
    void connectionEstablished(const ConnectionInfo &connection);
    void connectionTerminated(const ConnectionHistory &connection);
    void suspiciousActivityDetected(const QString &appName, const QString &reason);
    void protocolAnomalyDetected(const QString &protocol, const QString &details);

private:
#ifdef HAVE_PCAP
    static void packetHandler(u_char *user, const struct pcap_pkthdr *pkthdr, const u_char *packet);
    
    pcap_t *m_pcapHandle;
#endif
    bool m_isCapturing;
    QMap<qint64, NetworkStats> m_processStats; // Key: Process ID
    QMap<QString, NetworkStats> m_interfaceStats;
    QList<ConnectionInfo> m_activeConnections;
    QList<ConnectionHistory> m_connectionHistory;
    QMap<QString, bool> m_monitoredApplications;
    QMap<QString, quint64> m_protocolStats;
    QMap<QString, quint64> m_portStats;
    QMap<QString, QDateTime> m_lastActivity;
    QQueue<ConnectionInfo> m_recentConnections; // Keep last 1000 connections
    QString m_currentInterface; // Current interface being monitored
    
    mutable QMutex m_mutex; // For thread safety
    QFuture<void> m_captureFuture; // Store the background thread
    QFutureWatcher<void> *m_captureWatcher; // Monitor the background thread
    QTimer *m_updateTimer; // Timer for updating active connections
    QTimer *m_analysisTimer; // Timer for traffic analysis
    
#ifdef HAVE_PCAP
    void processPacket(const struct pcap_pkthdr *pkthdr, const u_char *packet);
#endif
    void updateActiveConnections();
    void updateConnectionHistory();
    void analyzeTrafficPatterns();
    void detectSuspiciousActivity();
    QString getProcessNameFromPid(qint64 pid) const;
    QIcon getProcessIcon(const QString &processPath) const;
    QString getProcessPathFromPid(qint64 pid) const;
    QString getServiceName(quint16 port) const;
    QString getHostnameFromIP(const QString &ip) const;
    bool isConnectionSuspicious(const ConnectionInfo &connection) const;
    void addToConnectionHistory(const ConnectionInfo &connection, const QString &reason);
    QList<ConnectionInfo> filterConnections(const QList<ConnectionInfo> &connections, 
                                          const ConnectionFilter &filter) const;
};

#endif // NETWORKMONITOR_H
