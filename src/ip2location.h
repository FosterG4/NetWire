#ifndef IP2LOCATION_H
#define IP2LOCATION_H

#include <QString>
#include <QMap>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QTimer>
#include <QByteArray>

class IP2Location : public QObject
{
    Q_OBJECT

public:
    struct LocationInfo {
        QString country;
        QString region;
        QString city;
        double latitude;
        double longitude;
        QString zipCode;
        QString timezone;
        QString isp;
        QString domain;
        QString netSpeed;
        QString iddCode;
        QString areaCode;
        QString weatherStationCode;
        QString weatherStationName;
        QString mcc;
        QString mnc;
        QString mobileBrand;
        QString elevation;
        QString usageType;
        
        LocationInfo() : latitude(0.0), longitude(0.0) {}
        
        QString toDisplayString() const {
            if (!city.isEmpty() && !country.isEmpty()) {
                return QString("%1, %2").arg(city).arg(country);
            } else if (!country.isEmpty()) {
                return country;
            }
            return "Unknown";
        }
        
        QString toDetailedString() const {
            QStringList parts;
            if (!city.isEmpty()) parts << city;
            if (!region.isEmpty()) parts << region;
            if (!country.isEmpty()) parts << country;
            if (!isp.isEmpty()) parts << QString("ISP: %1").arg(isp);
            if (!domain.isEmpty()) parts << QString("Domain: %1").arg(domain);
            
            return parts.join(", ");
        }
    };

    explicit IP2Location(QObject *parent = nullptr);
    ~IP2Location();

    // Get location info from IP address
    LocationInfo getLocationFromIP(const QString &ip) const;
    
    // Download and update the database
    void downloadDatabase();
    
    // Check if database is available and valid
    bool isDatabaseReady() const;
    
    // Get database file path
    QString getDatabasePath() const;
    
    // Get database info
    QString getDatabaseInfo() const;

signals:
    void databaseDownloadStarted();
    void databaseDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void databaseDownloadFinished(bool success);
    void databaseReady();

private slots:
    void onDownloadFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    // Database header structure
    struct DatabaseHeader {
        quint8 databaseType;
        quint8 databaseColumns;
        quint8 databaseDay;
        quint8 databaseMonth;
        quint16 databaseYear;
        quint32 databaseCount;
        quint32 databaseAddr;
        quint32 databaseIdx;
        quint32 databaseRange;
        quint32 databaseBaseAddr;
    };
    
    // IPv4 record structure
    struct IPv4Record {
        quint32 ipFrom;
        quint32 ipTo;
        quint32 countryPosition;
        quint32 regionPosition;
        quint32 cityPosition;
        quint32 ispPosition;
        quint32 latitude;
        quint32 longitude;
        quint8 domain;
        quint8 zipCode;
        quint8 timeZone;
        quint8 netSpeed;
        quint8 iddCode;
        quint8 areaCode;
        quint8 weatherStationCode;
        quint8 weatherStationName;
        quint8 mcc;
        quint8 mnc;
        quint8 mobileBrand;
        quint8 elevation;
        quint8 usageType;
    };
    
    // IPv6 record structure
    struct IPv6Record {
        QByteArray ipFrom;
        QByteArray ipTo;
        quint32 countryPosition;
        quint32 regionPosition;
        quint32 cityPosition;
        quint32 ispPosition;
        quint32 latitude;
        quint32 longitude;
        quint8 domain;
        quint8 zipCode;
        quint8 timeZone;
        quint8 netSpeed;
        quint8 iddCode;
        quint8 areaCode;
        quint8 weatherStationCode;
        quint8 weatherStationName;
        quint8 mcc;
        quint8 mnc;
        quint8 mobileBrand;
        quint8 elevation;
        quint8 usageType;
    };

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_currentDownload;
    QString m_databasePath;
    QString m_downloadUrl;
    QString m_apiToken;
    
    // Database data
    QByteArray m_databaseData;
    DatabaseHeader m_header;
    bool m_databaseLoaded;
    
    // Cache for faster lookups
    mutable QMap<QString, LocationInfo> m_locationCache;
    
    // Database parsing methods
    bool loadDatabase();
    bool parseDatabaseHeader();
    LocationInfo lookupIPv4(const QString &ip) const;
    LocationInfo lookupIPv6(const QString &ip) const;
    QString readString(quint32 position) const;
    quint32 readUInt32(quint32 position) const;
    quint16 readUInt16(quint32 position) const;
    quint8 readUInt8(quint32 position) const;
    bool isPrivateIP(const QString &ip) const;
    void initializeFallbackData();
    QByteArray ipv6ToBytes(const QString &ip) const;
    int compareIPv6(const QByteArray &ip1, const QByteArray &ip2) const;
};

#endif // IP2LOCATION_H
