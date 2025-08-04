#include "networkmonitor.h"
#include <QDebug>
#include <QNetworkInterface>
#include <QThread>
#include <QDateTime>
#include <QFileInfo>
#include <QDir>
#include <QApplication>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

// Windows headers for process information
#include <TlHelp32.h>
#include <psapi.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "psapi.lib")

// Structure to store IP header information
#pragma pack(push, 1)
typedef struct ip_header {
    unsigned char  ver_ihl;        // Version (4 bits) + Internet header length (4 bits)
    unsigned char  tos;            // Type of service
    unsigned short tlen;           // Total length
    unsigned short identification; // Identification
    unsigned short flags_fo;       // Flags (3 bits) + Fragment offset (13 bits)
    unsigned char  ttl;            // Time to live
    unsigned char  proto;          // Protocol
    unsigned short crc;            // Header checksum
    unsigned int   saddr;          // Source address
    unsigned int   daddr;          // Destination address
} ip_header;

// Structure to store TCP header
#pragma pack(push, 1)
typedef struct tcp_header {
    unsigned short sport;  // Source port
    unsigned short dport;  // Destination port
    unsigned int   seq;    // Sequence number
    unsigned int   ack;    // Acknowledgment number
    unsigned char  len_res;// Data offset + Reserved
    unsigned char  flags;  // Control bits
    unsigned short win;    // Window
    unsigned short sum;    // Checksum
    unsigned short urp;    // Urgent pointer
} tcp_header;

// Structure to store UDP header
#pragma pack(push, 1)
typedef struct udp_header {
    unsigned short sport;  // Source port
    unsigned short dport;  // Destination port
    unsigned short len;    // Length
    unsigned short crc;    // Checksum
} udp_header;

#pragma pack(pop)

NetworkMonitor::NetworkMonitor(QObject *parent)
    : QObject(parent)
    , m_pcapHandle(nullptr)
    , m_isCapturing(false)
{
    // Initialize Windows Sockets API (WSA)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        qWarning() << "WSAStartup failed";
    }
    
    // Start a timer to periodically update active connections
    QTimer *updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &NetworkMonitor::updateActiveConnections);
    updateTimer->start(2000); // Update every 2 seconds
}

NetworkMonitor::~NetworkMonitor()
{
    stopCapture();
    if (m_pcapHandle) {
        pcap_close(m_pcapHandle);
    }
    
    // Cleanup Windows Sockets API
    WSACleanup();
}

bool NetworkMonitor::initialize()
{
    char errbuf[PCAP_ERRBUF_SIZE];
    
    // Find all network devices
    pcap_if_t *alldevs;
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        qWarning() << "Error finding network devices:" << errbuf;
        return false;
    }
    
    // Free the device list as we just needed to check if we can access it
    pcap_freealldevs(alldevs);
    return true;
}

QStringList NetworkMonitor::getAvailableInterfaces() const
{
    QStringList interfaces;
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *alldevs;
    
    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        qWarning() << "Error getting network interfaces:" << errbuf;
        return interfaces;
    }
    
    // Add all available interfaces to the list
    for (pcap_if_t *d = alldevs; d != nullptr; d = d->next) {
        if (d->description) {
            interfaces.append(QString("%1 (%2)").arg(d->description).arg(d->name));
        } else {
            interfaces.append(d->name);
        }
    }
    
    pcap_freealldevs(alldevs);
    return interfaces;
}

bool NetworkMonitor::startCapture(const QString &interfaceName)
{
    if (m_isCapturing) {
        stopCapture();
    }
    
    char errbuf[PCAP_ERRBUF_SIZE];
    
    // Convert QString to char* for pcap
    QByteArray interfaceNameBytes = interfaceName.toUtf8();
    const char *device = interfaceNameBytes.constData();
    
    // Open the network interface for packet capture
    m_pcapHandle = pcap_open_live(device, BUFSIZ, 1, 1000, errbuf);
    if (!m_pcapHandle) {
        qWarning() << "Couldn't open device" << interfaceName << ":" << errbuf;
        return false;
    }
    
    // Set non-blocking mode
    if (pcap_setnonblock(m_pcapHandle, 1, errbuf) == -1) {
        qWarning() << "Failed to set non-blocking mode:" << errbuf;
        pcap_close(m_pcapHandle);
        m_pcapHandle = nullptr;
        return false;
    }
    
    // Start capturing in a separate thread
    m_isCapturing = true;
    QtConcurrent::run([this]() {
        struct pcap_pkthdr header;
        const u_char *packet;
        
        while (m_isCapturing) {
            // Process available packets
            while ((packet = pcap_next(m_pcapHandle, &header)) != nullptr) {
                processPacket(&header, packet);
            }
            
            // Small delay to prevent high CPU usage
            QThread::msleep(10);
        }
    });
    
    return true;
}

void NetworkMonitor::stopCapture()
{
    m_isCapturing = false;
    if (m_pcapHandle) {
        pcap_breakloop(m_pcapHandle);
    }
}

QMap<qint64, NetworkMonitor::NetworkStats> NetworkMonitor::getStats() const
{
    QMutexLocker locker(&m_mutex);
    return m_processStats;
}

QList<NetworkMonitor::ConnectionInfo> NetworkMonitor::getActiveConnections() const
{
    QMutexLocker locker(&m_mutex);
    return m_activeConnections;
}

QMap<QString, NetworkMonitor::NetworkStats> NetworkMonitor::getStatsByApplication() const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, NetworkStats> appStats;
    
    for (const auto &stats : m_processStats) {
        if (!stats.processName.isEmpty()) {
            appStats[stats.processName].bytesReceived += stats.bytesReceived;
            appStats[stats.processName].bytesSent += stats.bytesSent;
            appStats[stats.processName].packetsReceived += stats.packetsReceived;
            appStats[stats.processName].packetsSent += stats.packetsSent;
            
            // Only set process name and icon once
            if (appStats[stats.processName].processName.isEmpty()) {
                appStats[stats.processName].processName = stats.processName;
                appStats[stats.processName].processIcon = stats.processIcon;
                appStats[stats.processName].processId = stats.processId;
            }
        }
    }
    
    return appStats;
}

QString NetworkMonitor::getProcessNameFromPid(qint64 pid) const
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        return QString();
    }
    
    wchar_t processName[MAX_PATH] = L"<unknown>";
    if (GetModuleBaseNameW(hProcess, NULL, processName, MAX_PATH) == 0) {
        CloseHandle(hProcess);
        return QString();
    }
    
    CloseHandle(hProcess);
    return QString::fromWCharArray(processName);
}

QString NetworkMonitor::getProcessPathFromPid(qint64 pid) const
{
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess == NULL) {
        return QString();
    }
    
    wchar_t processPath[MAX_PATH];
    if (GetModuleFileNameExW(hProcess, NULL, processPath, MAX_PATH) == 0) {
        CloseHandle(hProcess);
        return QString();
    }
    
    CloseHandle(hProcess);
    return QString::fromWCharArray(processPath);
}

QIcon NetworkMonitor::getProcessIcon(const QString &processPath) const
{
    if (processPath.isEmpty()) {
        return QIcon();
    }
    
    // Get the icon from the executable
    SHFILEINFOW shfi = {0};
    if (SHGetFileInfoW(
        (LPCWSTR)processPath.utf16(),
        0,
        &shfi,
        sizeof(shfi),
        SHGFI_ICON | SHGFI_SMALLICON)) {
        
        QPixmap pixmap = QtWin::fromHICON(shfi.hIcon);
        DestroyIcon(shfi.hIcon);
        return QIcon(pixmap);
    }
    
    return QIcon();
}

void NetworkMonitor::updateActiveConnections()
{
    QMutexLocker locker(&m_mutex);
    m_activeConnections.clear();
    
    // Get TCP connections
    PMIB_TCPTABLE_OWNER_PID pTcpTable = NULL;
    DWORD dwSize = 0;
    
    if (GetExtendedTcpTable(pTcpTable, &dwSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER) {
        pTcpTable = (PMIB_TCPTABLE_OWNER_PID)malloc(dwSize);
        if (pTcpTable) {
            if (GetExtendedTcpTable(pTcpTable, &dwSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
                for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++) {
                    ConnectionInfo info;
                    info.localAddress = QString::fromStdString(inet_ntoa(*(in_addr*)&pTcpTable->table[i].dwLocalAddr));
                    info.localPort = ntohs((u_short)pTcpTable->table[i].dwLocalPort);
                    info.remoteAddress = QString::fromStdString(inet_ntoa(*(in_addr*)&pTcpTable->table[i].dwRemoteAddr));
                    info.remotePort = ntohs((u_short)pTcpTable->table[i].dwRemotePort);
                    info.protocol = 6; // TCP
                    info.processId = pTcpTable->table[i].dwOwningPid;
                    info.processName = getProcessNameFromPid(info.processId);
                    
                    m_activeConnections.append(info);
                }
            }
            free(pTcpTable);
        }
    }
    
    // Get UDP connections (similar to TCP)
    PMIB_UDP6TABLE_OWNER_PID pUdpTable = NULL;
    dwSize = 0;
    
    if (GetExtendedUdpTable(pUdpTable, &dwSize, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0) == ERROR_INSUFFICIENT_BUFFER) {
        pUdpTable = (PMIB_UDP6TABLE_OWNER_PID)malloc(dwSize);
        if (pUdpTable) {
            if (GetExtendedUdpTable(pUdpTable, &dwSize, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
                for (DWORD i = 0; i < pUdpTable->dwNumEntries; i++) {
                    ConnectionInfo info;
                    info.localAddress = QString::fromStdString(inet_ntoa(*(in_addr*)&pUdpTable->table[i].dwLocalAddr));
                    info.localPort = ntohs((u_short)pUdpTable->table[i].dwLocalPort);
                    info.remoteAddress = "*";
                    info.remotePort = 0;
                    info.protocol = 17; // UDP
                    info.processId = pUdpTable->table[i].dwOwningPid;
                    info.processName = getProcessNameFromPid(info.processId);
                    
                    m_activeConnections.append(info);
                }
            }
            free(pUdpTable);
        }
    }
}

NetworkMonitor::ConnectionInfo NetworkMonitor::getConnectionInfo(const QString &localAddr, quint16 localPort, 
                                                               const QString &remoteAddr, quint16 remotePort, int protocol) const
{
    QMutexLocker locker(&m_mutex);
    
    for (const auto &conn : m_activeConnections) {
        if (conn.localAddress == localAddr && 
            conn.localPort == localPort &&
            (conn.remoteAddress == remoteAddr || conn.remoteAddress == "*") &&
            (conn.remotePort == remotePort || conn.remotePort == 0) &&
            conn.protocol == protocol) {
            return conn;
        }
    }
    
    return ConnectionInfo();
}

void NetworkMonitor::processPacket(const struct pcap_pkthdr *pkthdr, const u_char *packet)
{
    QMutexLocker locker(&m_mutex);
    
    // Parse Ethernet header (assuming Ethernet)
    const u_char *ip_packet = packet + 14; // Skip Ethernet header (14 bytes)
    ip_header *ip_hdr = (ip_header*)ip_packet;
    
    // Check if this is an IP packet
    if ((ip_hdr->ver_ihl >> 4) != 4) {
        return; // Not IPv4
    }
    
    // Get protocol type
    int protocol = ip_hdr->proto;
    
    // Get source and destination IPs
    struct in_addr src_addr, dst_addr;
    src_addr.s_addr = ip_hdr->saddr;
    dst_addr.s_addr = ip_hdr->daddr;
    
    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(src_addr), src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(dst_addr), dst_ip, INET_ADDRSTRLEN);
    
    quint16 src_port = 0;
    quint16 dst_port = 0;
    
    // Parse TCP or UDP headers to get ports
    if (protocol == 6 || protocol == 17) { // TCP or UDP
        const u_char *transport_packet = ip_packet + ((ip_hdr->ver_ihl & 0x0F) * 4);
        
        if (protocol == 6) { // TCP
            tcp_header *tcp_hdr = (tcp_header*)transport_packet;
            src_port = ntohs(tcp_hdr->sport);
            dst_port = ntohs(tcp_hdr->dport);
        } else { // UDP
            udp_header *udp_hdr = (udp_header*)transport_packet;
            src_port = ntohs(udp_hdr->sport);
            dst_port = ntohs(udp_hdr->dport);
        }
        
        // Get process information for this connection
        ConnectionInfo connInfo = getConnectionInfo(src_ip, src_port, dst_ip, dst_port, protocol);
        if (connInfo.processId > 0) {
            // Update statistics for this process
            NetworkStats &stats = m_processStats[connInfo.processId];
            stats.bytesReceived += pkthdr->len;
            stats.packetsReceived++;
            
            if (stats.processName.isEmpty() && !connInfo.processName.isEmpty()) {
                stats.processName = connInfo.processName;
                stats.processId = connInfo.processId;
                stats.processIcon = getProcessIcon(getProcessPathFromPid(connInfo.processId));
            }
        }
    }
    
    // Emit signal to update UI periodically
    static QDateTime lastUpdate = QDateTime::currentDateTime();
    if (lastUpdate.msecsTo(QDateTime::currentDateTime()) >= 1000) {
        emit networkDataUpdated();
        lastUpdate = QDateTime::currentDateTime();
    }
}
