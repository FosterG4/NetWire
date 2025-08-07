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
#include <QFileIconProvider>
#include <QProcess>
#include <QProcessEnvironment>
#include <QtConcurrent/QtConcurrent>

// Platform-specific includes
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <shellapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")
#elif defined(Q_OS_LINUX)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#elif defined(Q_OS_MACOS)
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <libproc.h>
#endif

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
#pragma pack(pop)

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
#pragma pack(pop)

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
#ifdef HAVE_PCAP
    , m_pcapHandle(nullptr)
#endif
    , m_isCapturing(false)
    , m_captureWatcher(new QFutureWatcher<void>(this))
{
    // Initialize network on Windows
#ifdef Q_OS_WIN
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        qWarning() << "WSAStartup failed";
    }
#endif
    
    // Start a timer to periodically update active connections
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &NetworkMonitor::updateActiveConnections);
    m_updateTimer->start(2000); // Update every 2 seconds
}

NetworkMonitor::~NetworkMonitor()
{
    // Stop the update timer
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    
    // Stop capture and wait for background thread to finish
    stopCapture();
    
    // Wait for the background thread to finish if it's running
    if (m_captureFuture.isRunning()) {
        m_captureFuture.waitForFinished();
    }
    
#ifdef HAVE_PCAP
    if (m_pcapHandle) {
        pcap_close(m_pcapHandle);
    }
#endif
    
    // Cleanup Windows Sockets API
#ifdef Q_OS_WIN
    WSACleanup();
#endif
}

bool NetworkMonitor::initialize()
{
#ifdef HAVE_PCAP
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
#else
    qWarning() << "libpcap not available - network monitoring disabled";
    return false;
#endif
}

QStringList NetworkMonitor::getAvailableInterfaces() const
{
    QStringList interfaces;
#ifdef HAVE_PCAP
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
#else
    // Fallback to Qt network interfaces when pcap is not available
    QList<QNetworkInterface> qtInterfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &interface : qtInterfaces) {
        if (interface.isValid() && !interface.addressEntries().isEmpty()) {
            interfaces.append(interface.humanReadableName());
        }
    }
#endif
    return interfaces;
}

bool NetworkMonitor::startCapture(const QString &interfaceName)
{
    if (m_isCapturing) {
        stopCapture();
    }
    
#ifdef HAVE_PCAP
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
    m_captureFuture = QtConcurrent::run([this]() {
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
    
    // Connect the future to the watcher for monitoring
    m_captureWatcher->setFuture(m_captureFuture);
    
    return true;
#else
    qWarning() << "libpcap not available - cannot start capture";
    return false;
#endif
}

void NetworkMonitor::stopCapture()
{
#ifdef HAVE_PCAP
    m_isCapturing = false;
    if (m_pcapHandle) {
        pcap_breakloop(m_pcapHandle);
    }
#endif
    
    // Wait for the background thread to finish if it's running
    if (m_captureFuture.isRunning()) {
        m_captureFuture.waitForFinished();
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

void NetworkMonitor::updateNetworkStats()
{
    QMutexLocker locker(&m_mutex);
    
    // Calculate total download and upload rates
    quint64 totalDownload = 0;
    quint64 totalUpload = 0;
    
    for (const auto &stats : m_processStats) {
        totalDownload += stats.downloadRate;
        totalUpload += stats.uploadRate;
    }
    
    // Emit the statsUpdated signal with the aggregated data
    emit statsUpdated(totalDownload, totalUpload);
    
    // Emit connection count changed signal
    emit connectionCountChanged(m_activeConnections.size());
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
#ifdef Q_OS_WIN
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
#elif defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)
    QFileInfo info(QString("/proc/%1/comm").arg(pid));
    if (info.exists()) {
        QFile file(info.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly)) {
            return QString::fromUtf8(file.readAll().trimmed());
        }
    }
    return QString();
#elif defined(Q_OS_MACOS)
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, pathbuf, sizeof(pathbuf)) > 0) {
        return QFileInfo(QString::fromUtf8(pathbuf)).fileName();
    }
    return QString();
#else
    return QString();
#endif
}

QString NetworkMonitor::getProcessPathFromPid(qint64 pid) const
{
#ifdef Q_OS_WIN
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
#elif defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)
    QFileInfo exeInfo(QString("/proc/%1/exe").arg(pid));
    if (exeInfo.exists()) {
        return exeInfo.canonicalFilePath();
    }
    return QString();
#elif defined(Q_OS_MACOS)
    char pathbuf[PROC_PIDPATHINFO_MAXSIZE];
    if (proc_pidpath(pid, pathbuf, sizeof(pathbuf)) > 0) {
        return QString::fromUtf8(pathbuf);
    }
    return QString();
#else
    return QString();
#endif
}

QIcon NetworkMonitor::getProcessIcon(const QString &processPath) const
{
    if (processPath.isEmpty()) {
        return QIcon();
    }
    
    QFileInfo fileInfo(processPath);
    if (!fileInfo.exists()) {
        return QIcon();
    }
    
#ifdef Q_OS_WIN
    // Windows-specific icon extraction
    SHFILEINFOW shfi = {0};
    if (SHGetFileInfoW(
        (LPCWSTR)processPath.utf16(),
        0,
        &shfi,
        sizeof(shfi),
        SHGFI_ICON | SHGFI_SMALLICON)) {
        
        QPixmap pixmap = QPixmap::fromImage(QImage::fromHICON(shfi.hIcon));
        DestroyIcon(shfi.hIcon);
        return QIcon(pixmap);
    }
    return QIcon();
#else
    // On Unix-like systems, use QFileIconProvider which provides a cross-platform way to get file icons
    static QFileIconProvider iconProvider;
    return iconProvider.icon(fileInfo);
#endif
}

void NetworkMonitor::updateActiveConnections()
{
    QMutexLocker locker(&m_mutex);
    m_activeConnections.clear();

#ifdef Q_OS_WIN
    // Windows implementation using IP Helper API
    // Get TCP connections
    PMIB_TCPTABLE_OWNER_PID pTcpTable = NULL;
    DWORD dwSize = 0;
    
    if (GetExtendedTcpTable(pTcpTable, &dwSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == ERROR_INSUFFICIENT_BUFFER) {
        pTcpTable = (PMIB_TCPTABLE_OWNER_PID)malloc(dwSize);
        if (pTcpTable) {
            if (GetExtendedTcpTable(pTcpTable, &dwSize, TRUE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
                for (DWORD i = 0; i < pTcpTable->dwNumEntries; i++) {
                    ConnectionInfo info;
                    char addrStr[INET_ADDRSTRLEN];
                    if (inet_ntop(AF_INET, &pTcpTable->table[i].dwLocalAddr, addrStr, INET_ADDRSTRLEN)) {
                        info.localAddress = QString::fromStdString(addrStr);
                    }
                    info.localPort = ntohs((u_short)pTcpTable->table[i].dwLocalPort);
                    char remoteAddrStr[INET_ADDRSTRLEN];
                    if (inet_ntop(AF_INET, &pTcpTable->table[i].dwRemoteAddr, remoteAddrStr, INET_ADDRSTRLEN)) {
                        info.remoteAddress = QString::fromStdString(remoteAddrStr);
                    }
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
    
    // Get UDP connections
    PMIB_UDP6TABLE_OWNER_PID pUdpTable = NULL;
    dwSize = 0;
    
    if (GetExtendedUdpTable(pUdpTable, &dwSize, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0) == ERROR_INSUFFICIENT_BUFFER) {
        pUdpTable = (PMIB_UDP6TABLE_OWNER_PID)malloc(dwSize);
        if (pUdpTable) {
            if (GetExtendedUdpTable(pUdpTable, &dwSize, TRUE, AF_INET, UDP_TABLE_OWNER_PID, 0) == NO_ERROR) {
                for (DWORD i = 0; i < pUdpTable->dwNumEntries; i++) {
                    ConnectionInfo info;
                    // UDP IPv6 table uses ucLocalAddr instead of dwLocalAddr
                    char addrStr[INET6_ADDRSTRLEN];
                    if (inet_ntop(AF_INET6, pUdpTable->table[i].ucLocalAddr, addrStr, INET6_ADDRSTRLEN)) {
                        info.localAddress = QString::fromStdString(addrStr);
                    }
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
#elif defined(Q_OS_LINUX) || defined(Q_OS_ANDROID)
    // Linux implementation using /proc/net/tcp and /proc/net/udp
    QFile tcpFile("/proc/net/tcp");
    if (tcpFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&tcpFile);
        in.readLine(); // Skip header line
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 10) {
                ConnectionInfo info;
                
                // Parse local and remote addresses
                QStringList local = parts[1].split(':');
                QStringList remote = parts[2].split(':');
                
                if (local.size() == 2 && remote.size() == 2) {
                    bool ok;
                    info.localAddress = QHostAddress(local[0].toUInt(&ok, 16)).toString();
                    info.localPort = local[1].toUShort(&ok, 16);
                    info.remoteAddress = QHostAddress(remote[0].toUInt(&ok, 16)).toString();
                    info.remotePort = remote[1].toUShort(&ok, 16);
                    info.protocol = 6; // TCP
                    
                    // On Linux, getting the process ID requires additional steps (e.g., using lsof or ss)
                    // This is a simplified example
                    info.processId = -1;
                    info.processName = "";
                    
                    m_activeConnections.append(info);
                }
            }
        }
        tcpFile.close();
    }
    
    // Similar implementation for UDP
    QFile udpFile("/proc/net/udp");
    if (udpFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&udpFile);
        in.readLine(); // Skip header line
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            QStringList parts = line.split(' ', Qt::SkipEmptyParts);
            if (parts.size() >= 10) {
                ConnectionInfo info;
                
                // Parse local address
                QStringList local = parts[1].split(':');
                
                if (local.size() == 2) {
                    bool ok;
                    info.localAddress = QHostAddress(local[0].toUInt(&ok, 16)).toString();
                    info.localPort = local[1].toUShort(&ok, 16);
                    info.remoteAddress = "*";
                    info.remotePort = 0;
                    info.protocol = 17; // UDP
                    
                    // On Linux, getting the process ID requires additional steps
                    info.processId = -1;
                    info.processName = "";
                    
                    m_activeConnections.append(info);
                }
            }
        }
        udpFile.close();
    }
#elif defined(Q_OS_MACOS)
    // macOS implementation using netstat
    QProcess netstatProcess;
    netstatProcess.start("netstat", QStringList() << "-n" << "-p" << "tcp");
    if (netstatProcess.waitForFinished()) {
        QByteArray output = netstatProcess.readAllStandardOutput();
        QTextStream stream(output);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.startsWith("tcp")) {
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 5) {
                    ConnectionInfo info;
                    
                    // Parse local and remote addresses
                    QStringList local = parts[3].split('.');
                    QStringList remote = parts[4].split('.');
                    
                    if (local.size() >= 2 && remote.size() >= 2) {
                        info.localAddress = local[0];
                        info.localPort = local[1].toUShort();
                        info.remoteAddress = remote[0];
                        info.remotePort = remote[1].toUShort();
                        info.protocol = 6; // TCP
                        info.processId = -1;
                        info.processName = "";
                        
                        m_activeConnections.append(info);
                    }
                }
            }
        }
    }
    
    // Similar for UDP
    netstatProcess.start("netstat", QStringList() << "-n" << "-p" << "udp");
    if (netstatProcess.waitForFinished()) {
        QByteArray output = netstatProcess.readAllStandardOutput();
        QTextStream stream(output);
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.startsWith("udp")) {
                QStringList parts = line.split(' ', Qt::SkipEmptyParts);
                if (parts.size() >= 4) {
                    ConnectionInfo info;
                    
                    // Parse local address
                    QStringList local = parts[3].split('.');
                    
                    if (local.size() >= 2) {
                        info.localAddress = local[0];
                        info.localPort = local[1].toUShort();
                        info.remoteAddress = "*";
                        info.remotePort = 0;
                        info.protocol = 17; // UDP
                        info.processId = -1;
                        info.processName = "";
                        
                        m_activeConnections.append(info);
                    }
                }
            }
        }
    }
#else
    // Fallback implementation for other platforms
    qWarning() << "Network connection monitoring not implemented for this platform";
#endif
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

#ifdef HAVE_PCAP
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
#endif

QString NetworkMonitor::getApplicationPath(const QString &appName) const
{
    QMutexLocker locker(&m_mutex);
    
    // Search through process stats to find the application
    for (const auto &stats : m_processStats) {
        if (stats.processName == appName) {
            return getProcessPathFromPid(stats.processId);
        }
    }
    
    return QString();
}

NetworkMonitor::NetworkStats NetworkMonitor::getApplicationStats(const QString &appName) const
{
    QMutexLocker locker(&m_mutex);
    
    // Search through process stats to find the application
    for (const auto &stats : m_processStats) {
        if (stats.processName == appName) {
            return stats;
        }
    }
    
    return NetworkStats();
}
