#ifndef IPLOOKUP_H
#define IPLOOKUP_H

#include <QString>
#include <QMap>
#include <QHostAddress>

class IPLookup
{
public:
    IPLookup();
    
    // Get country from IP address using local database
    QString getCountryFromIP(const QString &ip) const;
    
    // Get ISP/Organization info from IP ranges
    QString getISPFromIP(const QString &ip) const;
    
    // Check if IP is in a specific country range
    bool isIPInCountry(const QString &ip, const QString &country) const;

private:
    struct IPRange {
        quint32 startIP;
        quint32 endIP;
        QString country;
        QString countryCode;
        QString organization;
    };
    
    QList<IPRange> m_ipRanges;
    QMap<QString, QString> m_countryNames; // Code to full name mapping
    
    void initializeIPDatabase();
    quint32 ipToInt(const QString &ip) const;
    bool isPrivateIP(const QString &ip) const;
    QString getWellKnownService(const QString &ip) const;
};

#endif // IPLOOKUP_H
