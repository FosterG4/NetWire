#include "intrusiondetectionmanager.h"
#include "globallogger.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSettings>
#include <QDir>
#include <QStandardPaths>
#include <QProcess>
#include <QRegularExpression>
#include <QHostInfo>
#include <QNetworkInterface>

IntrusionDetectionManager* IntrusionDetectionManager::m_instance = nullptr;

IntrusionDetectionManager::IntrusionDetectionManager(QObject *parent)
    : QObject(parent)
    , m_currentThreatLevel(None)
    , m_privacyScore(100)
    , m_dnsLeakDetected(false)
    , m_vpnConnected(false)
    , m_scanTimer(new QTimer(this))
    , m_threatIntelTimer(new QTimer(this))
    , m_networkManager(new QNetworkAccessManager(this))
{
    LOG_FUNCTION_ENTRY();
    
    // Set up file paths
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(configDir);
    m_signaturesFilePath = configDir + "/signatures.json";
    m_threatIntelFilePath = configDir + "/threat_intelligence.json";
    
    // Set up timers
    connect(m_scanTimer, &QTimer::timeout, this, &IntrusionDetectionManager::runSecurityScan);
    connect(m_threatIntelTimer, &QTimer::timeout, this, &IntrusionDetectionManager::updateThreatIntelligence);
    
    // Set up network manager
    connect(m_networkManager, &QNetworkAccessManager::finished, this, &IntrusionDetectionManager::onThreatIntelResponse);
    
    // Load configurations
    loadSignatures();
    loadThreatIntelligence();
    
    // Initialize with default signatures
    if (m_signatures.isEmpty()) {
        initializeDefaultSignatures();
    }
    
    LOG_DEBUG("IntrusionDetectionManager initialized");
}

IntrusionDetectionManager::~IntrusionDetectionManager()
{
    LOG_FUNCTION_ENTRY();
    saveSignatures();
    saveThreatIntelligence();
}

IntrusionDetectionManager* IntrusionDetectionManager::instance()
{
    if (!m_instance) {
        m_instance = new IntrusionDetectionManager();
    }
    return m_instance;
}

void IntrusionDetectionManager::startMonitoring()
{
    LOG_FUNCTION_ENTRY();
    
    // Start security scan timer (every 30 seconds)
    m_scanTimer->start(30000);
    
    // Start threat intelligence update timer (every 5 minutes)
    m_threatIntelTimer->start(300000);
    
    // Initial security scan
    runSecurityScan();
    
    LOG_DEBUG("Intrusion detection monitoring started");
}

void IntrusionDetectionManager::stopMonitoring()
{
    LOG_FUNCTION_ENTRY();
    
    m_scanTimer->stop();
    m_threatIntelTimer->stop();
    
    LOG_DEBUG("Intrusion detection monitoring stopped");
}

void IntrusionDetectionManager::processConnection(const NetworkMonitor::ConnectionInfo &conn)
{
    LOG_FUNCTION_ENTRY();
    
    // Perform signature-based detection
    performSignatureDetection(conn);
    
    // Perform anomaly detection
    performAnomalyDetection(conn);
    
    // Perform heuristic detection
    performHeuristicDetection(conn);
    
    // Check threat intelligence
    checkThreatIntelligence(conn.remoteAddress);
}

void IntrusionDetectionManager::performSignatureDetection(const NetworkMonitor::ConnectionInfo &conn)
{
    LOG_FUNCTION_ENTRY();
    
    for (const auto &signature : m_signatures) {
        if (!signature.enabled) continue;
        
        // Check if connection matches signature pattern
        QString connectionString = QString("%1:%2->%3:%4 %5")
            .arg(conn.localAddress)
            .arg(conn.localPort)
            .arg(conn.remoteAddress)
            .arg(conn.remotePort)
            .arg(conn.protocol == 6 ? "TCP" : "UDP");
        
        QRegularExpression regex(signature.pattern);
        if (regex.match(connectionString).hasMatch()) {
            // Create security event
            SecurityEvent event;
            event.id = generateEventId();
            event.type = SignatureBased;
            event.level = signature.level;
            event.title = QString("Signature Match: %1").arg(signature.name);
            event.description = signature.description;
            event.sourceIP = conn.localAddress;
            event.destinationIP = conn.remoteAddress;
            event.sourcePort = conn.localPort;
            event.destinationPort = conn.remotePort;
            event.protocol = conn.protocol == 6 ? "TCP" : "UDP";
            event.bytesTransferred = conn.bytesReceived + conn.bytesSent;
            event.timestamp = QDateTime::currentDateTime();
            event.blocked = false;
            event.signature = signature.id;
            event.additionalInfo = QString("Category: %1").arg(signature.category);
            
            m_recentEvents.append(event);
            emit securityEventDetected(event);
            
            LOG_WARNING(QString("Signature match detected: %1").arg(signature.name));
        }
    }
}

void IntrusionDetectionManager::performAnomalyDetection(const NetworkMonitor::ConnectionInfo &conn)
{
    LOG_FUNCTION_ENTRY();
    
    // Check for suspicious ports
    if (isSuspiciousPort(conn.remotePort)) {
        SecurityEvent event;
        event.id = generateEventId();
        event.type = AnomalyBased;
        event.level = Medium;
        event.title = "Suspicious Port Detected";
        event.description = QString("Connection to suspicious port %1").arg(conn.remotePort);
        event.sourceIP = conn.localAddress;
        event.destinationIP = conn.remoteAddress;
        event.sourcePort = conn.localPort;
        event.destinationPort = conn.remotePort;
        event.protocol = conn.protocol == 6 ? "TCP" : "UDP";
        event.bytesTransferred = conn.bytesReceived + conn.bytesSent;
        event.timestamp = QDateTime::currentDateTime();
        event.blocked = false;
        event.signature = "ANOMALY_SUSPICIOUS_PORT";
        event.additionalInfo = QString("Port: %1").arg(conn.remotePort);
        
        m_recentEvents.append(event);
        emit securityEventDetected(event);
        
        LOG_WARNING(QString("Suspicious port detected: %1").arg(conn.remotePort));
    }
    
    // Check for known malicious IPs
    if (isKnownMaliciousIP(conn.remoteAddress)) {
        SecurityEvent event;
        event.id = generateEventId();
        event.type = AnomalyBased;
        event.level = High;
        event.title = "Known Malicious IP Detected";
        event.description = QString("Connection to known malicious IP %1").arg(conn.remoteAddress);
        event.sourceIP = conn.localAddress;
        event.destinationIP = conn.remoteAddress;
        event.sourcePort = conn.localPort;
        event.destinationPort = conn.remotePort;
        event.protocol = conn.protocol == 6 ? "TCP" : "UDP";
        event.bytesTransferred = conn.bytesReceived + conn.bytesSent;
        event.timestamp = QDateTime::currentDateTime();
        event.blocked = true;
        event.signature = "ANOMALY_MALICIOUS_IP";
        event.additionalInfo = QString("IP: %1").arg(conn.remoteAddress);
        
        m_recentEvents.append(event);
        m_blockedEvents.append(event);
        emit securityEventDetected(event);
        
        // Block the IP
        blockIP(conn.remoteAddress, "Known malicious IP");
        
        LOG_WARNING(QString("Known malicious IP detected: %1").arg(conn.remoteAddress));
    }
}

void IntrusionDetectionManager::performHeuristicDetection(const NetworkMonitor::ConnectionInfo &conn)
{
    LOG_FUNCTION_ENTRY();
    
    // Check for unusual connection patterns
    // This is a simplified heuristic - in a real implementation, you'd have more sophisticated logic
    
    // Check for connections to non-standard ports for common protocols
    if (conn.remotePort == 22 && conn.protocol == 6) {
        // SSH connection - could be legitimate or suspicious
        // In a real implementation, you'd check if this is expected behavior
    }
    
    // Check for large data transfers to unknown destinations
    qint64 totalBytes = conn.bytesReceived + conn.bytesSent;
    if (totalBytes > 10 * 1024 * 1024) { // 10MB threshold
        SecurityEvent event;
        event.id = generateEventId();
        event.type = HeuristicBased;
        event.level = Medium;
        event.title = "Large Data Transfer Detected";
        event.description = QString("Large data transfer to %1").arg(conn.remoteAddress);
        event.sourceIP = conn.localAddress;
        event.destinationIP = conn.remoteAddress;
        event.sourcePort = conn.localPort;
        event.destinationPort = conn.remotePort;
        event.protocol = conn.protocol == 6 ? "TCP" : "UDP";
        event.bytesTransferred = totalBytes;
        event.timestamp = QDateTime::currentDateTime();
        event.blocked = false;
        event.signature = "HEURISTIC_LARGE_TRANSFER";
        event.additionalInfo = QString("Bytes: %1").arg(formatBytes(totalBytes));
        
        m_recentEvents.append(event);
        emit securityEventDetected(event);
        
        LOG_WARNING(QString("Large data transfer detected: %1 to %2").arg(formatBytes(totalBytes)).arg(conn.remoteAddress));
    }
}

void IntrusionDetectionManager::checkThreatIntelligence(const QString &ipAddress)
{
    LOG_FUNCTION_ENTRY();
    
    // Check local threat intelligence database
    for (const auto &threat : m_threatIntelligence) {
        if (threat.ipAddress == ipAddress) {
            SecurityEvent event;
            event.id = generateEventId();
            event.type = ThreatIntelBased;
            event.level = threat.confidence > 80 ? High : Medium;
            event.title = QString("Threat Intelligence Match: %1").arg(threat.threatType);
            event.description = threat.description;
            event.sourceIP = ""; // Will be filled by caller
            event.destinationIP = ipAddress;
            event.timestamp = QDateTime::currentDateTime();
            event.blocked = true;
            event.signature = "THREAT_INTEL_MATCH";
            event.additionalInfo = QString("Confidence: %1%, Source: %2").arg(threat.confidence).arg(threat.source);
            
            m_recentEvents.append(event);
            m_blockedEvents.append(event);
            emit securityEventDetected(event);
            
            // Block the IP
            blockIP(ipAddress, QString("Threat intelligence: %1").arg(threat.threatType));
            
            LOG_WARNING(QString("Threat intelligence match: %1 (%2)").arg(ipAddress).arg(threat.threatType));
            break;
        }
    }
}

bool IntrusionDetectionManager::checkDNSLeak()
{
    LOG_FUNCTION_ENTRY();
    
    // Get current DNS servers
    QList<QHostAddress> dnsServers;
    for (const auto &interface : QNetworkInterface::allInterfaces()) {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            
            for (const auto &address : interface.addressEntries()) {
                // Qt6 doesn't have dnsServer() method, so we'll use a different approach
                // For now, we'll check if the interface has any DNS servers configured
                // This is a simplified approach - in a real implementation, you'd query the system DNS
            }
        }
    }
    
    // Check if DNS servers are public (potential leak)
    // For now, we'll use a simplified approach since Qt6 doesn't provide direct DNS server access
    // In a real implementation, you'd query the system DNS configuration
    bool dnsLeak = false;
    
    // Simplified DNS leak detection - check if we're using common public DNS
    // This is a placeholder - in a real implementation, you'd query the actual DNS servers
    Q_UNUSED(dnsServers);
    
    m_dnsLeakDetected = dnsLeak;
    if (dnsLeak) {
        emit dnsLeakDetected();
        LOG_WARNING("DNS leak detected");
    }
    
    return dnsLeak;
}

bool IntrusionDetectionManager::checkVPNStatus()
{
    LOG_FUNCTION_ENTRY();
    
    // Check for VPN interfaces
    bool vpnFound = false;
    for (const auto &interface : QNetworkInterface::allInterfaces()) {
        QString interfaceName = interface.humanReadableName().toLower();
        if (interfaceName.contains("vpn") || interfaceName.contains("tun") || 
            interfaceName.contains("tap") || interfaceName.contains("ppp")) {
            vpnFound = true;
            break;
        }
    }
    
    m_vpnConnected = vpnFound;
    emit vpnStatusChanged(vpnFound);
    
    if (vpnFound) {
        LOG_DEBUG("VPN connection detected");
    } else {
        LOG_DEBUG("No VPN connection detected");
    }
    
    return vpnFound;
}

int IntrusionDetectionManager::calculatePrivacyScore() const
{
    LOG_FUNCTION_ENTRY();
    
    int score = 100;
    
    // Deduct points for privacy issues
    if (m_dnsLeakDetected) score -= 20;
    if (!m_vpnConnected) score -= 30;
    if (m_currentThreatLevel >= High) score -= 25;
    if (m_currentThreatLevel >= Medium) score -= 15;
    
    // Ensure score doesn't go below 0
    return qMax(0, score);
}

QStringList IntrusionDetectionManager::getPrivacyRecommendations() const
{
    LOG_FUNCTION_ENTRY();
    
    QStringList recommendations;
    
    if (m_dnsLeakDetected) {
        recommendations << "Use a VPN to prevent DNS leaks";
        recommendations << "Configure secure DNS servers";
    }
    
    if (!m_vpnConnected) {
        recommendations << "Connect to a VPN for enhanced privacy";
        recommendations << "Use a trusted VPN service";
    }
    
    if (m_currentThreatLevel >= Medium) {
        recommendations << "Review recent security events";
        recommendations << "Update firewall rules";
    }
    
    if (recommendations.isEmpty()) {
        recommendations << "Your privacy settings look good!";
    }
    
    return recommendations;
}

void IntrusionDetectionManager::blockIP(const QString &ipAddress, const QString &reason)
{
    LOG_FUNCTION_ENTRY();
    
    if (!m_blockedIPs.contains(ipAddress)) {
        m_blockedIPs.insert(ipAddress);
        emit ipBlocked(ipAddress, reason);
        LOG_WARNING(QString("IP blocked: %1 - %2").arg(ipAddress).arg(reason));
    }
}

void IntrusionDetectionManager::unblockIP(const QString &ipAddress)
{
    LOG_FUNCTION_ENTRY();
    
    if (m_blockedIPs.contains(ipAddress)) {
        m_blockedIPs.remove(ipAddress);
        emit ipUnblocked(ipAddress);
        LOG_DEBUG(QString("IP unblocked: %1").arg(ipAddress));
    }
}

QList<IntrusionDetectionManager::SecurityEvent> IntrusionDetectionManager::recentEvents() const
{
    return m_recentEvents;
}

QList<IntrusionDetectionManager::SecurityEvent> IntrusionDetectionManager::blockedEvents() const
{
    return m_blockedEvents;
}

IntrusionDetectionManager::ThreatLevel IntrusionDetectionManager::currentThreatLevel() const
{
    return m_currentThreatLevel;
}

int IntrusionDetectionManager::activeThreats() const
{
    return m_recentEvents.size();
}

QList<IntrusionDetectionManager::DetectionSignature> IntrusionDetectionManager::signatures() const
{
    return m_signatures;
}

void IntrusionDetectionManager::addSignature(const DetectionSignature &signature)
{
    m_signatures.append(signature);
    saveSignatures();
}

void IntrusionDetectionManager::removeSignature(const QString &signatureId)
{
    for (int i = 0; i < m_signatures.size(); ++i) {
        if (m_signatures[i].id == signatureId) {
            m_signatures.removeAt(i);
            break;
        }
    }
    saveSignatures();
}

void IntrusionDetectionManager::enableSignature(const QString &signatureId, bool enabled)
{
    for (auto &signature : m_signatures) {
        if (signature.id == signatureId) {
            signature.enabled = enabled;
            break;
        }
    }
    saveSignatures();
}

QList<IntrusionDetectionManager::ThreatIntel> IntrusionDetectionManager::threatIntelligence() const
{
    return m_threatIntelligence;
}

void IntrusionDetectionManager::updateThreatIntelligence()
{
    LOG_FUNCTION_ENTRY();
    fetchThreatIntelligence();
}

bool IntrusionDetectionManager::isIPThreat(const QString &ipAddress) const
{
    for (const auto &threat : m_threatIntelligence) {
        if (threat.ipAddress == ipAddress) {
            return true;
        }
    }
    return false;
}

IntrusionDetectionManager::ThreatIntel IntrusionDetectionManager::getThreatInfo(const QString &ipAddress) const
{
    for (const auto &threat : m_threatIntelligence) {
        if (threat.ipAddress == ipAddress) {
            return threat;
        }
    }
    return ThreatIntel{};
}

void IntrusionDetectionManager::runSecurityScan()
{
    LOG_FUNCTION_ENTRY();
    
    // Update privacy checks
    checkDNSLeak();
    checkVPNStatus();
    
    // Update privacy score
    int newScore = calculatePrivacyScore();
    if (newScore != m_privacyScore) {
        m_privacyScore = newScore;
        emit privacyScoreChanged(newScore);
    }
    
    // Update threat level based on recent events
    ThreatLevel newLevel = None;
    for (const auto &event : m_recentEvents) {
        if (event.timestamp.secsTo(QDateTime::currentDateTime()) < 3600) { // Last hour
            if (event.level > newLevel) {
                newLevel = event.level;
            }
        }
    }
    
    if (newLevel != m_currentThreatLevel) {
        m_currentThreatLevel = newLevel;
        emit threatLevelChanged(newLevel);
    }
    
    LOG_DEBUG(QString("Security scan completed. Threat level: %1, Privacy score: %2").arg(newLevel).arg(newScore));
}

void IntrusionDetectionManager::processTrafficData(quint64 download, quint64 upload)
{
    LOG_FUNCTION_ENTRY();
    // This method can be used for traffic-based anomaly detection
    // For now, it's a placeholder for future implementation
}

QString IntrusionDetectionManager::generateEventId() const
{
    return QString("evt_%1").arg(QDateTime::currentMSecsSinceEpoch());
}

bool IntrusionDetectionManager::isKnownMaliciousIP(const QString &ipAddress) const
{
    // This is a simplified check - in a real implementation, you'd have a database
    // of known malicious IPs or use a threat intelligence service
    
    // Check against local threat intelligence
    for (const auto &threat : m_threatIntelligence) {
        if (threat.ipAddress == ipAddress) {
            return true;
        }
    }
    
    return false;
}

bool IntrusionDetectionManager::isSuspiciousPort(quint16 port) const
{
    // Common suspicious ports
    QSet<quint16> suspiciousPorts = {
        22,    // SSH (if unexpected)
        23,    // Telnet
        25,    // SMTP
        110,   // POP3
        143,   // IMAP
        445,   // SMB
        1433,  // SQL Server
        3306,  // MySQL
        3389,  // RDP
        5432,  // PostgreSQL
        5900,  // VNC
        8080,  // HTTP Proxy
        8443    // HTTPS Proxy
    };
    
    return suspiciousPorts.contains(port);
}

bool IntrusionDetectionManager::isSuspiciousProtocol(const QString &protocol) const
{
    QString protocolLower = protocol.toLower();
    return protocolLower == "telnet" || protocolLower == "ftp" || protocolLower == "rsh";
}

QString IntrusionDetectionManager::formatBytes(qint64 bytes) const
{
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QString("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QString("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
}

void IntrusionDetectionManager::loadSignatures()
{
    LOG_FUNCTION_ENTRY();
    
    QFile file(m_signaturesFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_DEBUG("No signatures file found, will create default signatures");
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray signaturesArray = doc.array();
    
    m_signatures.clear();
    for (const auto &signatureValue : signaturesArray) {
        QJsonObject signatureObj = signatureValue.toObject();
        DetectionSignature signature;
        signature.id = signatureObj["id"].toString();
        signature.name = signatureObj["name"].toString();
        signature.pattern = signatureObj["pattern"].toString();
        signature.description = signatureObj["description"].toString();
        signature.level = static_cast<ThreatLevel>(signatureObj["level"].toInt());
        signature.enabled = signatureObj["enabled"].toBool();
        signature.category = signatureObj["category"].toString();
        m_signatures.append(signature);
    }
    
    LOG_DEBUG(QString("Loaded %1 signatures").arg(m_signatures.size()));
}

void IntrusionDetectionManager::saveSignatures()
{
    LOG_FUNCTION_ENTRY();
    
    QJsonArray signaturesArray;
    for (const auto &signature : m_signatures) {
        QJsonObject signatureObj;
        signatureObj["id"] = signature.id;
        signatureObj["name"] = signature.name;
        signatureObj["pattern"] = signature.pattern;
        signatureObj["description"] = signature.description;
        signatureObj["level"] = static_cast<int>(signature.level);
        signatureObj["enabled"] = signature.enabled;
        signatureObj["category"] = signature.category;
        signaturesArray.append(signatureObj);
    }
    
    QJsonDocument doc(signaturesArray);
    QFile file(m_signaturesFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        LOG_DEBUG(QString("Saved %1 signatures").arg(m_signatures.size()));
    }
}

void IntrusionDetectionManager::loadThreatIntelligence()
{
    LOG_FUNCTION_ENTRY();
    
    QFile file(m_threatIntelFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_DEBUG("No threat intelligence file found");
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonArray threatsArray = doc.array();
    
    m_threatIntelligence.clear();
    for (const auto &threatValue : threatsArray) {
        QJsonObject threatObj = threatValue.toObject();
        ThreatIntel threat;
        threat.ipAddress = threatObj["ipAddress"].toString();
        threat.threatType = threatObj["threatType"].toString();
        threat.description = threatObj["description"].toString();
        threat.confidence = threatObj["confidence"].toInt();
        threat.lastSeen = QDateTime::fromString(threatObj["lastSeen"].toString(), Qt::ISODate);
        threat.source = threatObj["source"].toString();
        m_threatIntelligence.append(threat);
    }
    
    LOG_DEBUG(QString("Loaded %1 threat intelligence records").arg(m_threatIntelligence.size()));
}

void IntrusionDetectionManager::saveThreatIntelligence()
{
    LOG_FUNCTION_ENTRY();
    
    QJsonArray threatsArray;
    for (const auto &threat : m_threatIntelligence) {
        QJsonObject threatObj;
        threatObj["ipAddress"] = threat.ipAddress;
        threatObj["threatType"] = threat.threatType;
        threatObj["description"] = threat.description;
        threatObj["confidence"] = threat.confidence;
        threatObj["lastSeen"] = threat.lastSeen.toString(Qt::ISODate);
        threatObj["source"] = threat.source;
        threatsArray.append(threatObj);
    }
    
    QJsonDocument doc(threatsArray);
    QFile file(m_threatIntelFilePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        LOG_DEBUG(QString("Saved %1 threat intelligence records").arg(m_threatIntelligence.size()));
    }
}

void IntrusionDetectionManager::fetchThreatIntelligence()
{
    LOG_FUNCTION_ENTRY();
    
    // This is a placeholder for threat intelligence API calls
    // In a real implementation, you'd make API calls to threat intelligence services
    // For now, we'll just log that the update was attempted
    
    LOG_DEBUG("Threat intelligence update requested (placeholder)");
}

void IntrusionDetectionManager::onThreatIntelResponse(QNetworkReply *reply)
{
    LOG_FUNCTION_ENTRY();
    
    if (reply->error() == QNetworkReply::NoError) {
        // Process threat intelligence response
        // This is a placeholder for processing API responses
        LOG_DEBUG("Threat intelligence response received");
    } else {
        LOG_ERROR(QString("Threat intelligence request failed: %1").arg(reply->errorString()));
    }
    
    reply->deleteLater();
}

void IntrusionDetectionManager::initializeDefaultSignatures()
{
    LOG_FUNCTION_ENTRY();
    
    // Add some default signatures for common threats
    DetectionSignature sig1;
    sig1.id = "SIG_001";
    sig1.name = "Suspicious Port Scan";
    sig1.pattern = ".*:(22|23|25|80|443|3389|8080).*";
    sig1.description = "Connection to commonly scanned ports";
    sig1.level = Medium;
    sig1.enabled = true;
    sig1.category = "Port Scan";
    m_signatures.append(sig1);
    
    DetectionSignature sig2;
    sig2.id = "SIG_002";
    sig2.name = "Large Data Transfer";
    sig2.pattern = ".*";
    sig2.description = "Large data transfer detected";
    sig2.level = Low;
    sig2.enabled = true;
    sig2.category = "Data Transfer";
    m_signatures.append(sig2);
    
    DetectionSignature sig3;
    sig3.id = "SIG_003";
    sig3.name = "Suspicious Protocol";
    sig3.pattern = ".*(telnet|ftp|rsh).*";
    sig3.description = "Use of insecure protocols";
    sig3.level = High;
    sig3.enabled = true;
    sig3.category = "Protocol";
    m_signatures.append(sig3);
    
    saveSignatures();
    LOG_DEBUG("Initialized default signatures");
}
