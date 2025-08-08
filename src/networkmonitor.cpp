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
#include <QTextStream>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHostInfo>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>

// Platform-specific includes
#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <shellapi.h>
#include <tlhelp32.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "kernel32.lib")
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
    , m_networkManager(new QNetworkAccessManager(this))
    , m_ipLookup(new IPLookup())
    , m_ip2Location(new IP2Location(this))
{
    // Initialize network on Windows
#ifdef Q_OS_WIN
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        qWarning() << "WSAStartup failed";
    }
#endif

    // Set up timers with reduced frequency to improve performance
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(3000); // Update every 3 seconds (reduced from 1 second)
    connect(m_updateTimer, &QTimer::timeout, this, &NetworkMonitor::updateActiveConnections);
    
    m_analysisTimer = new QTimer(this);
    m_analysisTimer->setInterval(10000); // Analyze every 10 seconds (reduced frequency)
    connect(m_analysisTimer, &QTimer::timeout, this, &NetworkMonitor::analyzeTrafficPatterns);
    
    // Connect capture watcher
    connect(m_captureWatcher, &QFutureWatcher<void>::finished, this, [this]() {
        qDebug() << "Packet capture finished";
    });
    
    // Initialize connection tracking
    m_activeConnections.clear();
    m_connectionHistory.clear();
    m_processStats.clear();
    m_protocolStats.clear();
    m_portStats.clear();
    
    // Start timers
    m_updateTimer->start();
    m_analysisTimer->start();
    
    // Connect IP2Location signals and forward them
    connect(m_ip2Location, &IP2Location::databaseDownloadStarted, this, [this]() {
        qDebug() << "IP2Location database download started";
        emit databaseDownloadStarted();
    });
    
    connect(m_ip2Location, &IP2Location::databaseDownloadProgress, this, [this](qint64 bytesReceived, qint64 bytesTotal) {
        emit databaseDownloadProgress(bytesReceived, bytesTotal);
    });
    
    connect(m_ip2Location, &IP2Location::databaseDownloadFinished, this, [this](bool success) {
        qDebug() << "IP2Location database download" << (success ? "completed" : "failed");
        emit databaseDownloadFinished(success);
    });
    
    connect(m_ip2Location, &IP2Location::databaseReady, this, [this]() {
        qDebug() << "IP2Location database ready for use";
        emit databaseReady();
    });
    
    // Auto-download database if not available
    if (!m_ip2Location->isDatabaseReady()) {
        QTimer::singleShot(5000, this, [this]() { // Delay download by 5 seconds
            m_ip2Location->downloadDatabase();
        });
    }
}

NetworkMonitor::~NetworkMonitor()
{
    // Stop all timers
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    if (m_analysisTimer) {
        m_analysisTimer->stop();
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
    
    // Clear caches to free memory
    m_hostnameCache.clear();
    m_countryCache.clear();
    m_activeConnections.clear();
    m_connectionHistory.clear();
    m_processStats.clear();
    m_protocolStats.clear();
    m_portStats.clear();
    
    // Clean up IP lookup
    delete m_ipLookup;
    m_ipLookup = nullptr;
    
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
    // Try multiple methods to get process name, starting with most privileged
    
    // Method 1: Try with PROCESS_QUERY_LIMITED_INFORMATION (works better with admin rights)
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess != NULL) {
        DWORD dwSize = MAX_PATH;
        wchar_t processName[MAX_PATH];
        if (QueryFullProcessImageNameW(hProcess, 0, processName, &dwSize)) {
            CloseHandle(hProcess);
            QString fullPath = QString::fromWCharArray(processName);
            return QFileInfo(fullPath).fileName(); // Extract just the filename
        }
        CloseHandle(hProcess);
    }
    
    // Method 2: Try with PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess != NULL) {
        wchar_t processName[MAX_PATH] = L"<unknown>";
        if (GetModuleBaseNameW(hProcess, NULL, processName, MAX_PATH) > 0) {
            CloseHandle(hProcess);
            return QString::fromWCharArray(processName);
        }
        CloseHandle(hProcess);
    }
    
    // Method 3: Use CreateToolhelp32Snapshot (doesn't require special privileges)
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe32;
        pe32.dwSize = sizeof(PROCESSENTRY32W);
        
        if (Process32FirstW(hSnapshot, &pe32)) {
            do {
                if (pe32.th32ProcessID == static_cast<DWORD>(pid)) {
                    CloseHandle(hSnapshot);
                    return QString::fromWCharArray(pe32.szExeFile);
                }
            } while (Process32NextW(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }
    
    // If all methods fail, return a formatted PID string
    return QString("PID:%1").arg(pid);
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
    // Try multiple methods to get process path
    
    // Method 1: Try with PROCESS_QUERY_LIMITED_INFORMATION (works better with admin rights)
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (hProcess != NULL) {
        DWORD dwSize = MAX_PATH;
        wchar_t processPath[MAX_PATH];
        if (QueryFullProcessImageNameW(hProcess, 0, processPath, &dwSize)) {
            CloseHandle(hProcess);
            return QString::fromWCharArray(processPath);
        }
        CloseHandle(hProcess);
    }
    
    // Method 2: Try with PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
    hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (hProcess != NULL) {
        wchar_t processPath[MAX_PATH];
        if (GetModuleFileNameExW(hProcess, NULL, processPath, MAX_PATH) > 0) {
            CloseHandle(hProcess);
            return QString::fromWCharArray(processPath);
        }
        CloseHandle(hProcess);
    }
    
    // If both methods fail, return empty string
    return QString();
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
                    info.processPath = getProcessPathFromPid(info.processId);
                    info.processIcon = getProcessIcon(info.processPath);
                    info.connectionTime = QDateTime::currentDateTime();
                    info.lastActivity = QDateTime::currentDateTime();
                    
                    // Determine connection state
                    DWORD state = pTcpTable->table[i].dwState;
                    switch (state) {
                        case MIB_TCP_STATE_CLOSED: info.connectionState = "CLOSED"; break;
                        case MIB_TCP_STATE_LISTEN: info.connectionState = "LISTENING"; break;
                        case MIB_TCP_STATE_SYN_SENT: info.connectionState = "SYN_SENT"; break;
                        case MIB_TCP_STATE_SYN_RCVD: info.connectionState = "SYN_RECEIVED"; break;
                        case MIB_TCP_STATE_ESTAB: info.connectionState = "ESTABLISHED"; break;
                        case MIB_TCP_STATE_FIN_WAIT1: info.connectionState = "FIN_WAIT1"; break;
                        case MIB_TCP_STATE_FIN_WAIT2: info.connectionState = "FIN_WAIT2"; break;
                        case MIB_TCP_STATE_CLOSE_WAIT: info.connectionState = "CLOSE_WAIT"; break;
                        case MIB_TCP_STATE_CLOSING: info.connectionState = "CLOSING"; break;
                        case MIB_TCP_STATE_LAST_ACK: info.connectionState = "LAST_ACK"; break;
                        case MIB_TCP_STATE_TIME_WAIT: info.connectionState = "TIME_WAIT"; break;
                        case MIB_TCP_STATE_DELETE_TCB: info.connectionState = "DELETE_TCB"; break;
                        default: info.connectionState = "UNKNOWN"; break;
                    }
                    
                    // Get service name and traffic type
                    info.serviceName = getTrafficType(info.remotePort, info.protocol);
                    
                    // Resolve hostname and country (asynchronously)
                    if (!info.remoteAddress.isEmpty() && info.remoteAddress != "0.0.0.0") {
                        resolveHostname(info.remoteAddress);
                        // Get hostname from cache if available
                        if (m_hostnameCache.contains(info.remoteAddress)) {
                            info.remoteHostname = m_hostnameCache.value(info.remoteAddress);
                        }
                    }
                    
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
                    info.processPath = getProcessPathFromPid(info.processId);
                    info.processIcon = getProcessIcon(info.processPath);
                    info.connectionTime = QDateTime::currentDateTime();
                    info.lastActivity = QDateTime::currentDateTime();
                    info.connectionState = "LISTENING"; // UDP is connectionless
                    
                    // Get service name and traffic type
                    info.serviceName = getTrafficType(info.localPort, info.protocol);
                    
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
        ConnectionInfo connInfo = getConnectionDetails(src_ip, src_port, dst_ip, dst_port, protocol);
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

// Phase 2: Enhanced Connection Management

QList<NetworkMonitor::ConnectionInfo> NetworkMonitor::getFilteredConnections(const ConnectionFilter &filter) const
{
    QMutexLocker locker(&m_mutex);
    return filterConnections(m_activeConnections, filter);
}

QList<NetworkMonitor::ConnectionHistory> NetworkMonitor::getConnectionHistory(const ConnectionFilter &filter) const
{
    QMutexLocker locker(&m_mutex);
    QList<ConnectionHistory> filteredHistory;
    
    for (const auto &history : m_connectionHistory) {
        bool matches = true;
        
        if (!filter.processName.isEmpty() && history.processName != filter.processName) {
            matches = false;
        }
        if (!filter.localAddress.isEmpty() && history.localAddress != filter.localAddress) {
            matches = false;
        }
        if (!filter.remoteAddress.isEmpty() && history.remoteAddress != filter.remoteAddress) {
            matches = false;
        }
        if (filter.protocol != -1 && history.protocol != filter.protocol) {
            matches = false;
        }
        if (filter.startTime.isValid() && history.startTime < filter.startTime) {
            matches = false;
        }
        if (filter.endTime.isValid() && history.endTime > filter.endTime) {
            matches = false;
        }
        
        if (matches) {
            filteredHistory.append(history);
        }
    }
    
    return filteredHistory;
}

NetworkMonitor::ConnectionInfo NetworkMonitor::getConnectionDetails(const QString &localAddr, quint16 localPort, 
                                                                 const QString &remoteAddr, quint16 remotePort, int protocol) const
{
    QMutexLocker locker(&m_mutex);
    
    for (const auto &conn : m_activeConnections) {
        if (conn.localAddress == localAddr && conn.localPort == localPort &&
            conn.remoteAddress == remoteAddr && conn.remotePort == remotePort &&
            conn.protocol == protocol) {
            return conn;
        }
    }
    
    return ConnectionInfo();
}

bool NetworkMonitor::terminateConnection(const QString &localAddr, quint16 localPort, 
                                       const QString &remoteAddr, quint16 remotePort, int protocol)
{
#ifdef Q_OS_WIN
    // Find the connection and terminate it using Windows API
    for (const auto &conn : m_activeConnections) {
        if (conn.localAddress == localAddr && conn.localPort == localPort &&
            conn.remoteAddress == remoteAddr && conn.remotePort == remotePort &&
            conn.protocol == protocol) {
            
            // Use netsh to terminate the connection
            QProcess process;
            QString command = QString("netsh interface ip delete destinationcache");
            process.start("cmd", QStringList() << "/c" << command);
            process.waitForFinished();
            
            // Add to history
            ConnectionHistory history;
            history.localAddress = localAddr;
            history.localPort = localPort;
            history.remoteAddress = remoteAddr;
            history.remotePort = remotePort;
            history.protocol = protocol;
            history.processId = conn.processId;
            history.processName = conn.processName;
            history.startTime = conn.connectionTime;
            history.endTime = QDateTime::currentDateTime();
            history.terminationReason = "User";
            
            m_connectionHistory.append(history);
            emit connectionTerminated(history);
            
            return true;
        }
    }
#endif
    return false;
}

void NetworkMonitor::exportConnectionData(const QString &filename, const ConnectionFilter &filter)
{
    QMutexLocker locker(&m_mutex);
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Could not open file for writing:" << filename;
        return;
    }
    
    QTextStream out(&file);
    out << "Local Address,Local Port,Remote Address,Remote Port,Protocol,Process Name,Process ID,Connection Time,Last Activity,Bytes Received,Bytes Sent,Connection State\n";
    
    QList<ConnectionInfo> connections = filterConnections(m_activeConnections, filter);
    for (const auto &conn : connections) {
        out << QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12\n")
               .arg(conn.localAddress)
               .arg(conn.localPort)
               .arg(conn.remoteAddress)
               .arg(conn.remotePort)
               .arg(conn.protocol == 6 ? "TCP" : "UDP")
               .arg(conn.processName)
               .arg(conn.processId)
               .arg(conn.connectionTime.toString("yyyy-MM-dd hh:mm:ss"))
               .arg(conn.lastActivity.toString("yyyy-MM-dd hh:mm:ss"))
               .arg(conn.bytesReceived)
               .arg(conn.bytesSent)
               .arg(conn.connectionState);
    }
    
    file.close();
}

QMap<QString, quint64> NetworkMonitor::getConnectionStatistics() const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, quint64> stats;
    
    stats["Total Connections"] = m_activeConnections.size();
    stats["TCP Connections"] = 0;
    stats["UDP Connections"] = 0;
    stats["Total Bytes Received"] = 0;
    stats["Total Bytes Sent"] = 0;
    
    for (const auto &conn : m_activeConnections) {
        if (conn.protocol == 6) {
            stats["TCP Connections"]++;
        } else if (conn.protocol == 17) {
            stats["UDP Connections"]++;
        }
        stats["Total Bytes Received"] += conn.bytesReceived;
        stats["Total Bytes Sent"] += conn.bytesSent;
    }
    
    return stats;
}

// Phase 2: Application Profiling

QMap<QString, NetworkMonitor::NetworkStats> NetworkMonitor::getApplicationProfiles() const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, NetworkStats> profiles;
    
    // Group stats by application name
    for (const auto &stats : m_processStats) {
        if (!stats.processName.isEmpty()) {
            if (profiles.contains(stats.processName)) {
                // Aggregate stats for the same application
                NetworkStats &profile = profiles[stats.processName];
                profile.bytesReceived += stats.bytesReceived;
                profile.bytesSent += stats.bytesSent;
                profile.packetsReceived += stats.packetsReceived;
                profile.packetsSent += stats.packetsSent;
                profile.downloadRate += stats.downloadRate;
                profile.uploadRate += stats.uploadRate;
                profile.downloadTotal += stats.downloadTotal;
                profile.uploadTotal += stats.uploadTotal;
                profile.totalDownloaded += stats.totalDownloaded;
                profile.totalUploaded += stats.totalUploaded;
            } else {
                profiles[stats.processName] = stats;
            }
        }
    }
    
    return profiles;
}

QList<QString> NetworkMonitor::getSuspiciousApplications() const
{
    QMutexLocker locker(&m_mutex);
    QList<QString> suspiciousApps;
    
    // Check for suspicious patterns
    for (const auto &conn : m_activeConnections) {
        if (isConnectionSuspicious(conn)) {
            if (!suspiciousApps.contains(conn.processName)) {
                suspiciousApps.append(conn.processName);
            }
        }
    }
    
    return suspiciousApps;
}

QMap<QString, QList<NetworkMonitor::ConnectionInfo>> NetworkMonitor::getApplicationConnections() const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, QList<ConnectionInfo>> appConnections;
    
    for (const auto &conn : m_activeConnections) {
        if (!conn.processName.isEmpty()) {
            appConnections[conn.processName].append(conn);
        }
    }
    
    return appConnections;
}

void NetworkMonitor::setApplicationMonitoring(const QString &appName, bool enabled)
{
    QMutexLocker locker(&m_mutex);
    m_monitoredApplications[appName] = enabled;
}

bool NetworkMonitor::isApplicationMonitored(const QString &appName) const
{
    QMutexLocker locker(&m_mutex);
    return m_monitoredApplications.value(appName, true); // Default to true for all apps
}

// Phase 2: Traffic Analysis

QMap<QString, quint64> NetworkMonitor::getProtocolStatistics() const
{
    QMutexLocker locker(&m_mutex);
    return m_protocolStats;
}

QMap<QString, quint64> NetworkMonitor::getPortStatistics() const
{
    QMutexLocker locker(&m_mutex);
    return m_portStats;
}

QList<QString> NetworkMonitor::getTopTalkers() const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, quint64> talkerStats;
    
    for (const auto &conn : m_activeConnections) {
        QString key = conn.remoteAddress;
        talkerStats[key] += conn.bytesReceived + conn.bytesSent;
    }
    
    // Sort by traffic volume
    QList<QString> topTalkers;
    QList<quint64> volumes = talkerStats.values();
    std::sort(volumes.begin(), volumes.end(), std::greater<quint64>());
    
    for (const auto &volume : volumes) {
        for (auto it = talkerStats.begin(); it != talkerStats.end(); ++it) {
            if (it.value() == volume && !topTalkers.contains(it.key())) {
                topTalkers.append(it.key());
                break;
            }
        }
    }
    
    return topTalkers.mid(0, 10); // Return top 10
}

QList<QString> NetworkMonitor::getTopListeners() const
{
    QMutexLocker locker(&m_mutex);
    QMap<QString, quint64> listenerStats;
    
    for (const auto &conn : m_activeConnections) {
        if (conn.connectionState == "LISTENING") {
            QString key = QString("%1:%2").arg(conn.localAddress).arg(conn.localPort);
            listenerStats[key]++;
        }
    }
    
    // Sort by connection count
    QList<QString> topListeners;
    QList<quint64> counts = listenerStats.values();
    std::sort(counts.begin(), counts.end(), std::greater<quint64>());
    
    for (const auto &count : counts) {
        for (auto it = listenerStats.begin(); it != listenerStats.end(); ++it) {
            if (it.value() == count && !topListeners.contains(it.key())) {
                topListeners.append(it.key());
                break;
            }
        }
    }
    
    return topListeners.mid(0, 10); // Return top 10
}

double NetworkMonitor::getNetworkLatency(const QString &host) const
{
    // Simple ping implementation for latency measurement
    QProcess pingProcess;
    pingProcess.start("ping", QStringList() << "-n" << "1" << host);
    pingProcess.waitForFinished(5000); // 5 second timeout
    
    QString output = pingProcess.readAllStandardOutput();
    QRegularExpression timeRegex(R"(time[=<](\d+)ms)");
    QRegularExpressionMatch match = timeRegex.match(output);
    
    if (match.hasMatch()) {
        return match.captured(1).toDouble();
    }
    
    return -1.0; // Failed to measure
}

quint64 NetworkMonitor::getPacketLossRate() const
{
    // Calculate packet loss rate based on captured packets
    QMutexLocker locker(&m_mutex);
    
    quint64 totalPackets = 0;
    quint64 lostPackets = 0;
    
    for (const auto &stats : m_processStats) {
        totalPackets += stats.packetsReceived + stats.packetsSent;
        // Estimate lost packets (this is a simplified calculation)
        if (stats.packetsReceived > 0) {
            lostPackets += stats.packetsReceived * 0.01; // Assume 1% loss rate
        }
    }
    
    if (totalPackets == 0) return 0;
    return (lostPackets * 100) / totalPackets; // Return as percentage
}

// Private helper methods

void NetworkMonitor::updateConnectionHistory()
{
    QMutexLocker locker(&m_mutex);
    
    // Check for terminated connections
    QDateTime now = QDateTime::currentDateTime();
    QList<ConnectionInfo> activeConnections = m_activeConnections;
    
    for (const auto &conn : activeConnections) {
        // Check if connection is still active (simplified logic)
        if (conn.lastActivity.msecsTo(now) > 300000) { // 5 minutes timeout
            addToConnectionHistory(conn, "Timeout");
        }
    }
}

void NetworkMonitor::analyzeTrafficPatterns()
{
    QMutexLocker locker(&m_mutex);
    
    // Update protocol statistics
    m_protocolStats.clear();
    m_portStats.clear();
    
    for (const auto &conn : m_activeConnections) {
        QString protocol = conn.protocol == 6 ? "TCP" : "UDP";
        m_protocolStats[protocol] += conn.bytesReceived + conn.bytesSent;
        
        QString portKey = QString("%1").arg(conn.localPort);
        m_portStats[portKey] += conn.bytesReceived + conn.bytesSent;
    }
    
    // Detect suspicious activity
    detectSuspiciousActivity();
}

void NetworkMonitor::detectSuspiciousActivity()
{
    for (const auto &conn : m_activeConnections) {
        if (isConnectionSuspicious(conn)) {
            emit suspiciousActivityDetected(conn.processName, "Suspicious connection pattern");
        }
    }
}

QString NetworkMonitor::getServiceName(quint16 port) const
{
    // Common service names
    static QMap<quint16, QString> serviceNames = {
        {21, "FTP"}, {22, "SSH"}, {23, "Telnet"}, {25, "SMTP"},
        {53, "DNS"}, {80, "HTTP"}, {110, "POP3"}, {143, "IMAP"},
        {443, "HTTPS"}, {993, "IMAPS"}, {995, "POP3S"}, {1433, "MSSQL"},
        {3306, "MySQL"}, {5432, "PostgreSQL"}, {8080, "HTTP-Proxy"},
        {8443, "HTTPS-Alt"}, {27017, "MongoDB"}, {6379, "Redis"}
    };
    
    return serviceNames.value(port, QString());
}

QString NetworkMonitor::getHostnameFromIP(const QString &ip) const
{
    // Simple reverse DNS lookup
    QHostAddress addr(ip);
    if (addr.isNull()) return QString();
    
    QHostInfo info = QHostInfo::fromName(ip);
    if (info.error() == QHostInfo::NoError) {
        return info.hostName();
    }
    
    return QString();
}

bool NetworkMonitor::isConnectionSuspicious(const ConnectionInfo &connection) const
{
    // Check for suspicious patterns
    if (connection.remotePort == 22 && connection.protocol == 6) {
        // SSH connections to unknown hosts
        if (connection.remoteAddress != "127.0.0.1" && 
            !connection.remoteAddress.startsWith("192.168.") &&
            !connection.remoteAddress.startsWith("10.")) {
            return true;
        }
    }
    
    // High bandwidth connections
    if ((connection.bytesReceived + connection.bytesSent) > 100 * 1024 * 1024) { // 100MB
        return true;
    }
    
    // Connections to known malicious ports
    QList<quint16> suspiciousPorts = {6667, 6668, 6669, 31337, 12345, 54321};
    if (suspiciousPorts.contains(connection.remotePort)) {
        return true;
    }
    
    return false;
}

void NetworkMonitor::addToConnectionHistory(const ConnectionInfo &connection, const QString &reason)
{
    ConnectionHistory history;
    history.localAddress = connection.localAddress;
    history.localPort = connection.localPort;
    history.remoteAddress = connection.remoteAddress;
    history.remotePort = connection.remotePort;
    history.protocol = connection.protocol;
    history.processId = connection.processId;
    history.processName = connection.processName;
    history.startTime = connection.connectionTime;
    history.endTime = QDateTime::currentDateTime();
    history.totalBytesReceived = connection.bytesReceived;
    history.totalBytesSent = connection.bytesSent;
    history.terminationReason = reason;
    
    m_connectionHistory.append(history);
    
    // Keep only last 1000 history entries
    if (m_connectionHistory.size() > 1000) {
        m_connectionHistory.removeFirst();
    }
}

QList<NetworkMonitor::ConnectionInfo> NetworkMonitor::filterConnections(const QList<ConnectionInfo> &connections, 
                                                                      const ConnectionFilter &filter) const
{
    QList<ConnectionInfo> filtered;
    
    for (const auto &conn : connections) {
        bool matches = true;
        
        if (!filter.processName.isEmpty() && conn.processName != filter.processName) {
            matches = false;
        }
        if (!filter.localAddress.isEmpty() && conn.localAddress != filter.localAddress) {
            matches = false;
        }
        if (!filter.remoteAddress.isEmpty() && conn.remoteAddress != filter.remoteAddress) {
            matches = false;
        }
        if (filter.protocol != -1 && conn.protocol != filter.protocol) {
            matches = false;
        }
        if (!filter.connectionState.isEmpty() && conn.connectionState != filter.connectionState) {
            matches = false;
        }
        if (filter.showActiveOnly && conn.connectionState != "ESTABLISHED") {
            matches = false;
        }
        if (filter.startTime.isValid() && conn.connectionTime < filter.startTime) {
            matches = false;
        }
        if (filter.endTime.isValid() && conn.lastActivity > filter.endTime) {
            matches = false;
        }
        
        if (matches) {
            filtered.append(conn);
        }
    }
    
    return filtered;
}

// Enhanced monitoring features implementation
QString NetworkMonitor::getTrafficType(quint16 port, int protocol) const
{
    // Common port mappings for traffic type identification
    static const QMap<quint16, QString> tcpPorts = {
        {20, "FTP-Data"}, {21, "FTP"}, {22, "SSH"}, {23, "Telnet"},
        {25, "SMTP"}, {53, "DNS"}, {80, "HTTP"}, {110, "POP3"},
        {143, "IMAP"}, {443, "HTTPS"}, {993, "IMAPS"}, {995, "POP3S"},
        {465, "SMTPS"}, {587, "SMTP"}, {990, "FTPS"}, {3389, "RDP"},
        {5432, "PostgreSQL"}, {3306, "MySQL"}, {1433, "MSSQL"},
        {6379, "Redis"}, {27017, "MongoDB"}, {8080, "HTTP-Alt"},
        {8443, "HTTPS-Alt"}, {9200, "Elasticsearch"}, {5672, "AMQP"},
        {6667, "IRC"}, {194, "IRC"}, {6697, "IRC-SSL"},
        {1935, "RTMP"}, {554, "RTSP"}, {5060, "SIP"}, {5061, "SIP-TLS"}
    };
    
    static const QMap<quint16, QString> udpPorts = {
        {53, "DNS"}, {67, "DHCP-Server"}, {68, "DHCP-Client"},
        {69, "TFTP"}, {123, "NTP"}, {161, "SNMP"}, {162, "SNMP-Trap"},
        {514, "Syslog"}, {1194, "OpenVPN"}, {1701, "L2TP"},
        {4500, "IPSec"}, {500, "IPSec"}, {1812, "RADIUS"},
        {1813, "RADIUS-Accounting"}, {5353, "mDNS"},
        {137, "NetBIOS-NS"}, {138, "NetBIOS-DGM"}, {139, "NetBIOS-SSN"}
    };
    
    if (protocol == 6) { // TCP
        return tcpPorts.value(port, QString("TCP-%1").arg(port));
    } else if (protocol == 17) { // UDP
        return udpPorts.value(port, QString("UDP-%1").arg(port));
    }
    
    return QString("Unknown-%1").arg(port);
}

QString NetworkMonitor::getCountryFromIP(const QString &ip) const
{
    // Check cache first
    if (m_countryCache.contains(ip)) {
        return m_countryCache.value(ip);
    }
    
    QString country;
    
    // Try IP2Location first (more accurate, supports both IPv4 and IPv6)
    if (m_ip2Location && m_ip2Location->isDatabaseReady()) {
        IP2Location::LocationInfo location = m_ip2Location->getLocationFromIP(ip);
        country = location.toDisplayString();
        
        // If we got a valid result, cache it
        if (!country.isEmpty() && country != "Unknown") {
            m_countryCache[ip] = country;
            return country;
        }
    }
    
    // Fallback to basic IP lookup for IPv4 only
    QHostAddress addr(ip);
    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
        country = m_ipLookup->getCountryFromIP(ip);
    } else {
        country = "Unknown (IPv6)";
    }
    
    // Cache the result for future lookups
    m_countryCache[ip] = country;
    
    return country;
}

void NetworkMonitor::resolveHostname(const QString &ip)
{
    // Check cache first
    if (m_hostnameCache.contains(ip)) {
        return;
    }
    
    // Don't resolve local IPs
    QHostAddress addr(ip);
    if (addr.isLoopback() || addr.toString().startsWith("192.168.") || 
        addr.toString().startsWith("10.") || addr.toString().startsWith("172.")) {
        m_hostnameCache[ip] = ip;
        return;
    }
    
    // Disable hostname resolution to prevent crashes and improve performance
    // TODO: Re-enable with proper error handling and rate limiting
    m_hostnameCache[ip] = ip;
}

QMap<QString, QString> NetworkMonitor::getHostnameCache() const
{
    QMutexLocker locker(&m_mutex);
    return m_hostnameCache;
}

// IP2Location integration methods
IP2Location::LocationInfo NetworkMonitor::getDetailedLocationFromIP(const QString &ip) const
{
    if (m_ip2Location && m_ip2Location->isDatabaseReady()) {
        IP2Location::LocationInfo location = m_ip2Location->getLocationFromIP(ip);
        
        // If we got a valid result, return it
        if (!location.country.isEmpty() && location.country != "Unknown") {
            return location;
        }
    }
    
    // Return empty location info if IP2Location is not available
    IP2Location::LocationInfo emptyInfo;
    emptyInfo.country = "Unknown";
    
    // For IPv6 addresses, indicate this
    QHostAddress addr(ip);
    if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
        emptyInfo.country = "Unknown (IPv6)";
    }
    
    return emptyInfo;
}

void NetworkMonitor::downloadIP2LocationDatabase()
{
    if (m_ip2Location) {
        m_ip2Location->downloadDatabase();
    }
}

bool NetworkMonitor::isIP2LocationReady() const
{
    return m_ip2Location && m_ip2Location->isDatabaseReady();
}

QString NetworkMonitor::getIP2LocationDatabaseInfo() const
{
    if (m_ip2Location) {
        return m_ip2Location->getDatabaseInfo();
    }
    return "IP2Location not initialized";
}
