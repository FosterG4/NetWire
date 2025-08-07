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

signals:
    void networkDataUpdated();
    void statsUpdated(quint64 download, quint64 upload);
    void connectionCountChanged(int count);

private:
#ifdef HAVE_PCAP
    static void packetHandler(u_char *user, const struct pcap_pkthdr *pkthdr, const u_char *packet);
    
    pcap_t *m_pcapHandle;
#endif
    bool m_isCapturing;
    QMap<qint64, NetworkStats> m_processStats; // Key: Process ID
    QMap<QString, NetworkStats> m_interfaceStats;
    QList<ConnectionInfo> m_activeConnections;
    mutable QMutex m_mutex; // For thread safety
    QFuture<void> m_captureFuture; // Store the background thread
    QFutureWatcher<void> *m_captureWatcher; // Monitor the background thread
    QTimer *m_updateTimer; // Timer for updating active connections
    
#ifdef HAVE_PCAP
    void processPacket(const struct pcap_pkthdr *pkthdr, const u_char *packet);
#endif
    QString getProcessNameFromPid(qint64 pid) const;
    QIcon getProcessIcon(const QString &processPath) const;
    QString getProcessPathFromPid(qint64 pid) const;
    void updateActiveConnections();
    ConnectionInfo getConnectionInfo(const QString &localAddr, quint16 localPort, 
                                   const QString &remoteAddr, quint16 remotePort, int protocol) const;
};

#endif // NETWORKMONITOR_H
