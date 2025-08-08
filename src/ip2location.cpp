#include "ip2location.h"
#include <QNetworkRequest>
#include <QUrl>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QDebug>
#include <QFileInfo>
#include <QHostAddress>
#include <QDataStream>
#include <QDateTime>
#include <algorithm>

IP2Location::IP2Location(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_currentDownload(nullptr)
    , m_databaseLoaded(false)
{
    // Set up the download URL for DB11LITEBINIPV6
    m_apiToken = "9gAAl67KYk4QosQbdkNp8EuRD86YHNVDp4Ox7HmsrEVrFTwB4ykXyZtkHaoXWMCz";
    m_downloadUrl = QString("https://www.ip2location.com/download/?token=%1&file=DB11LITEBINIPV6").arg(m_apiToken);
    
    // Set up database path in application data directory
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir appDir(appDataPath);
    if (!appDir.exists()) {
        appDir.mkpath(".");
    }
    m_databasePath = appDir.filePath("IP2LOCATION-LITE-DB11-IPV6.BIN");
    
    // Initialize with fallback data for immediate use
    initializeFallbackData();
    
    // Try to load existing database
    if (QFile::exists(m_databasePath)) {
        loadDatabase();
    }
}

IP2Location::~IP2Location()
{
    if (m_currentDownload) {
        m_currentDownload->abort();
        m_currentDownload->deleteLater();
    }
}

IP2Location::LocationInfo IP2Location::getLocationFromIP(const QString &ip) const
{
    // Check cache first
    if (m_locationCache.contains(ip)) {
        return m_locationCache.value(ip);
    }
    
    // Check for private IPs
    if (isPrivateIP(ip)) {
        LocationInfo localInfo;
        localInfo.country = "Local";
        localInfo.city = "Private Network";
        m_locationCache[ip] = localInfo;
        return localInfo;
    }
    
    LocationInfo result;
    QHostAddress addr(ip);
    
    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
        result = lookupIPv4(ip);
    } else if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
        result = lookupIPv6(ip);
    }
    
    // Cache the result
    m_locationCache[ip] = result;
    return result;
}

void IP2Location::downloadDatabase()
{
    if (m_currentDownload) {
        qDebug() << "Download already in progress";
        return;
    }
    
    qDebug() << "Starting IP2Location DB11LITEBINIPV6 database download...";
    emit databaseDownloadStarted();
    
    QNetworkRequest request;
    request.setUrl(QUrl(m_downloadUrl));
    request.setRawHeader("User-Agent", "NetWire/1.0");
    
    m_currentDownload = m_networkManager->get(request);
    
    connect(m_currentDownload, &QNetworkReply::downloadProgress,
            this, &IP2Location::onDownloadProgress);
    connect(m_currentDownload, &QNetworkReply::finished,
            this, &IP2Location::onDownloadFinished);
}

bool IP2Location::isDatabaseReady() const
{
    return m_databaseLoaded && !m_databaseData.isEmpty();
}

QString IP2Location::getDatabasePath() const
{
    return m_databasePath;
}

QString IP2Location::getDatabaseInfo() const
{
    if (!m_databaseLoaded) {
        return "Database not loaded";
    }
    
    return QString("DB11LITEBINIPV6 - Type: %1, Columns: %2, Records: %3, Date: %4/%5/%6")
            .arg(m_header.databaseType)
            .arg(m_header.databaseColumns)
            .arg(m_header.databaseCount)
            .arg(m_header.databaseDay)
            .arg(m_header.databaseMonth)
            .arg(m_header.databaseYear);
}

void IP2Location::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit databaseDownloadProgress(bytesReceived, bytesTotal);
}

void IP2Location::onDownloadFinished()
{
    bool success = false;
    
    if (m_currentDownload->error() == QNetworkReply::NoError) {
        QFile file(m_databasePath);
        if (file.open(QIODevice::WriteOnly)) {
            QByteArray data = m_currentDownload->readAll();
            if (file.write(data) == data.size()) {
                file.close();
                success = loadDatabase();
                if (success) {
                    qDebug() << "IP2Location database downloaded and loaded successfully";
                } else {
                    qDebug() << "Failed to load downloaded database";
                }
            } else {
                qDebug() << "Failed to write database file";
            }
        } else {
            qDebug() << "Failed to open database file for writing";
        }
    } else {
        qDebug() << "Download failed:" << m_currentDownload->errorString();
    }
    
    m_currentDownload->deleteLater();
    m_currentDownload = nullptr;
    
    emit databaseDownloadFinished(success);
    if (success) {
        emit databaseReady();
    }
}

bool IP2Location::loadDatabase()
{
    QFile file(m_databasePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "Failed to open database file:" << m_databasePath;
        return false;
    }
    
    m_databaseData = file.readAll();
    file.close();
    
    if (m_databaseData.isEmpty()) {
        qDebug() << "Database file is empty";
        return false;
    }
    
    if (!parseDatabaseHeader()) {
        qDebug() << "Failed to parse database header";
        return false;
    }
    
    m_databaseLoaded = true;
    m_locationCache.clear(); // Clear cache when database is reloaded
    
    qDebug() << "Database loaded successfully:" << getDatabaseInfo();
    return true;
}

bool IP2Location::parseDatabaseHeader()
{
    if (m_databaseData.size() < 32) {
        return false;
    }
    
    // Read header (first 32 bytes)
    m_header.databaseType = readUInt8(0);
    m_header.databaseColumns = readUInt8(1);
    m_header.databaseDay = readUInt8(2);
    m_header.databaseMonth = readUInt8(3);
    m_header.databaseYear = readUInt16(4);
    m_header.databaseCount = readUInt32(6);
    m_header.databaseAddr = readUInt32(10);
    m_header.databaseIdx = readUInt32(14);
    m_header.databaseRange = readUInt32(18);
    m_header.databaseBaseAddr = readUInt32(22);
    
    // Validate header
    if (m_header.databaseType != 11) {
        qDebug() << "Invalid database type:" << m_header.databaseType;
        return false;
    }
    
    if (m_header.databaseColumns < 20) {
        qDebug() << "Invalid database columns:" << m_header.databaseColumns;
        return false;
    }
    
    return true;
}

IP2Location::LocationInfo IP2Location::lookupIPv4(const QString &ip) const
{
    if (!m_databaseLoaded) {
        return LocationInfo();
    }
    
    quint32 ipInt = QHostAddress(ip).toIPv4Address();
    
    // Binary search in IPv4 records
    quint32 left = 0;
    quint32 right = m_header.databaseCount - 1;
    
    while (left <= right) {
        quint32 mid = (left + right) / 2;
        quint32 recordOffset = m_header.databaseAddr + (mid * 20); // 20 bytes per record
        
        if (recordOffset + 20 > static_cast<quint32>(m_databaseData.size())) {
            break;
        }
        
        quint32 ipFrom = readUInt32(recordOffset);
        quint32 ipTo = readUInt32(recordOffset + 4);
        
        if (ipInt >= ipFrom && ipInt <= ipTo) {
            // Found matching record
            LocationInfo info;
            info.country = readString(readUInt32(recordOffset + 8));
            info.region = readString(readUInt32(recordOffset + 12));
            info.city = readString(readUInt32(recordOffset + 16));
            info.isp = readString(readUInt32(recordOffset + 20));
            info.latitude = static_cast<double>(readUInt32(recordOffset + 24)) / 10000.0 - 180.0;
            info.longitude = static_cast<double>(readUInt32(recordOffset + 28)) / 10000.0 - 180.0;
            info.domain = readString(readUInt8(recordOffset + 32));
            info.zipCode = readString(readUInt8(recordOffset + 33));
            info.timezone = readString(readUInt8(recordOffset + 34));
            info.netSpeed = readString(readUInt8(recordOffset + 35));
            info.iddCode = readString(readUInt8(recordOffset + 36));
            info.areaCode = readString(readUInt8(recordOffset + 37));
            info.weatherStationCode = readString(readUInt8(recordOffset + 38));
            info.weatherStationName = readString(readUInt8(recordOffset + 39));
            info.mcc = readString(readUInt8(recordOffset + 40));
            info.mnc = readString(readUInt8(recordOffset + 41));
            info.mobileBrand = readString(readUInt8(recordOffset + 42));
            info.elevation = readString(readUInt8(recordOffset + 43));
            info.usageType = readString(readUInt8(recordOffset + 44));
            
            return info;
        } else if (ipInt < ipFrom) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    
    return LocationInfo();
}

IP2Location::LocationInfo IP2Location::lookupIPv6(const QString &ip) const
{
    if (!m_databaseLoaded) {
        return LocationInfo();
    }
    
    QByteArray ipBytes = ipv6ToBytes(ip);
    
    // Binary search in IPv6 records (stored after IPv4 records)
    quint32 left = 0;
    quint32 right = m_header.databaseCount - 1;
    quint32 ipv6StartOffset = m_header.databaseAddr + (m_header.databaseCount * 20); // Skip IPv4 records
    
    while (left <= right) {
        quint32 mid = (left + right) / 2;
        quint32 recordOffset = ipv6StartOffset + (mid * 40); // 40 bytes per IPv6 record
        
        if (recordOffset + 40 > static_cast<quint32>(m_databaseData.size())) {
            break;
        }
        
        QByteArray ipFrom = m_databaseData.mid(recordOffset, 16);
        QByteArray ipTo = m_databaseData.mid(recordOffset + 16, 16);
        
        int fromCompare = compareIPv6(ipBytes, ipFrom);
        int toCompare = compareIPv6(ipBytes, ipTo);
        
        if (fromCompare >= 0 && toCompare <= 0) {
            // Found matching record
            LocationInfo info;
            info.country = readString(readUInt32(recordOffset + 32));
            info.region = readString(readUInt32(recordOffset + 36));
            info.city = readString(readUInt32(recordOffset + 40));
            info.isp = readString(readUInt32(recordOffset + 44));
            info.latitude = static_cast<double>(readUInt32(recordOffset + 48)) / 10000.0 - 180.0;
            info.longitude = static_cast<double>(readUInt32(recordOffset + 52)) / 10000.0 - 180.0;
            info.domain = readString(readUInt8(recordOffset + 56));
            info.zipCode = readString(readUInt8(recordOffset + 57));
            info.timezone = readString(readUInt8(recordOffset + 58));
            info.netSpeed = readString(readUInt8(recordOffset + 59));
            info.iddCode = readString(readUInt8(recordOffset + 60));
            info.areaCode = readString(readUInt8(recordOffset + 61));
            info.weatherStationCode = readString(readUInt8(recordOffset + 62));
            info.weatherStationName = readString(readUInt8(recordOffset + 63));
            info.mcc = readString(readUInt8(recordOffset + 64));
            info.mnc = readString(readUInt8(recordOffset + 65));
            info.mobileBrand = readString(readUInt8(recordOffset + 66));
            info.elevation = readString(readUInt8(recordOffset + 67));
            info.usageType = readString(readUInt8(recordOffset + 68));
            
            return info;
        } else if (fromCompare < 0) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    
    return LocationInfo();
}

QString IP2Location::readString(quint32 position) const
{
    if (position >= static_cast<quint32>(m_databaseData.size())) {
        return QString();
    }
    
    // Read string from position until null terminator
    QByteArray data = m_databaseData.mid(position);
    int nullPos = data.indexOf('\0');
    if (nullPos >= 0) {
        data = data.left(nullPos);
    }
    
    return QString::fromUtf8(data);
}

quint32 IP2Location::readUInt32(quint32 position) const
{
    if (position + 4 > static_cast<quint32>(m_databaseData.size())) {
        return 0;
    }
    
    QDataStream stream(m_databaseData.mid(position, 4));
    stream.setByteOrder(QDataStream::LittleEndian);
    quint32 value;
    stream >> value;
    return value;
}

quint16 IP2Location::readUInt16(quint32 position) const
{
    if (position + 2 > static_cast<quint32>(m_databaseData.size())) {
        return 0;
    }
    
    QDataStream stream(m_databaseData.mid(position, 2));
    stream.setByteOrder(QDataStream::LittleEndian);
    quint16 value;
    stream >> value;
    return value;
}

quint8 IP2Location::readUInt8(quint32 position) const
{
    if (position >= static_cast<quint32>(m_databaseData.size())) {
        return 0;
    }
    
    return static_cast<quint8>(m_databaseData.at(position));
}

bool IP2Location::isPrivateIP(const QString &ip) const
{
    QHostAddress addr(ip);
    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
        quint32 ipInt = addr.toIPv4Address();
        // Private IP ranges
        return (ipInt >= 0x0A000000 && ipInt <= 0x0AFFFFFF) || // 10.0.0.0/8
               (ipInt >= 0xAC100000 && ipInt <= 0xAC1FFFFF) || // 172.16.0.0/12
               (ipInt >= 0xC0A80000 && ipInt <= 0xC0A8FFFF) || // 192.168.0.0/16
               (ipInt >= 0x7F000000 && ipInt <= 0x7FFFFFFF);   // 127.0.0.0/8
    } else if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
        // IPv6 private ranges
        QString ipStr = addr.toString();
        return ipStr.startsWith("fc00:") || ipStr.startsWith("fd00:") || // Unique Local
               ipStr.startsWith("fe80:") || ipStr.startsWith("::1");     // Link Local, Loopback
    }
    return false;
}

QByteArray IP2Location::ipv6ToBytes(const QString &ip) const
{
    QHostAddress addr(ip);
    if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
        Q_IPV6ADDR ipv6 = addr.toIPv6Address();
        return QByteArray(reinterpret_cast<const char*>(ipv6.c), 16);
    }
    return QByteArray();
}

int IP2Location::compareIPv6(const QByteArray &ip1, const QByteArray &ip2) const
{
    if (ip1.size() != 16 || ip2.size() != 16) {
        return 0;
    }
    
    for (int i = 0; i < 16; ++i) {
        if (ip1[i] < ip2[i]) return -1;
        if (ip1[i] > ip2[i]) return 1;
    }
    return 0;
}

void IP2Location::initializeFallbackData()
{
    // Initialize with basic fallback data for immediate use
    // This will be used until the database is downloaded
    m_databaseLoaded = false;
    m_locationCache.clear();
}
