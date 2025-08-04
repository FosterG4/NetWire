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
#ifdef HAVE_PCAP
#include <pcap.h>
#endif
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

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
        
        NetworkStats() : bytesReceived(0), bytesSent(0), 
                        packetsReceived(0), packetsSent(0), 
                        processId(-1) {}
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
    QMap<qint64, NetworkStats> getStats() const;
    QList<ConnectionInfo> getActiveConnections() const;
    QMap<QString, NetworkStats> getStatsByApplication() const;

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
