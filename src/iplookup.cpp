#include "iplookup.h"
#include <QHostAddress>
#include <QDebug>

IPLookup::IPLookup()
{
    initializeIPDatabase();
}

QString IPLookup::getCountryFromIP(const QString &ip) const
{
    // Check for private/local IPs first
    if (isPrivateIP(ip)) {
        return "Local";
    }
    
    // Check for well-known services
    QString wellKnown = getWellKnownService(ip);
    if (!wellKnown.isEmpty()) {
        return wellKnown;
    }
    
    // Convert IP to integer for range checking
    quint32 ipInt = ipToInt(ip);
    if (ipInt == 0) {
        return "Invalid";
    }
    
    // Binary search through IP ranges for better performance
    int left = 0;
    int right = m_ipRanges.size() - 1;
    
    while (left <= right) {
        int mid = (left + right) / 2;
        const IPRange &range = m_ipRanges[mid];
        
        if (ipInt >= range.startIP && ipInt <= range.endIP) {
            return range.country;
        } else if (ipInt < range.startIP) {
            right = mid - 1;
        } else {
            left = mid + 1;
        }
    }
    
    return "Unknown";
}

QString IPLookup::getISPFromIP(const QString &ip) const
{
    // Check for well-known services first
    QString wellKnown = getWellKnownService(ip);
    if (!wellKnown.isEmpty()) {
        return wellKnown;
    }
    
    quint32 ipInt = ipToInt(ip);
    if (ipInt == 0) {
        return "Unknown";
    }
    
    // Look for organization info in our database
    for (const IPRange &range : m_ipRanges) {
        if (ipInt >= range.startIP && ipInt <= range.endIP) {
            return range.organization.isEmpty() ? "Unknown ISP" : range.organization;
        }
    }
    
    return "Unknown ISP";
}

bool IPLookup::isIPInCountry(const QString &ip, const QString &country) const
{
    return getCountryFromIP(ip).compare(country, Qt::CaseInsensitive) == 0;
}

void IPLookup::initializeIPDatabase()
{
    // Initialize country code to name mapping
    m_countryNames["US"] = "United States";
    m_countryNames["CN"] = "China";
    m_countryNames["JP"] = "Japan";
    m_countryNames["DE"] = "Germany";
    m_countryNames["GB"] = "United Kingdom";
    m_countryNames["FR"] = "France";
    m_countryNames["CA"] = "Canada";
    m_countryNames["AU"] = "Australia";
    m_countryNames["RU"] = "Russia";
    m_countryNames["BR"] = "Brazil";
    m_countryNames["IN"] = "India";
    m_countryNames["KR"] = "South Korea";
    m_countryNames["IT"] = "Italy";
    m_countryNames["ES"] = "Spain";
    m_countryNames["NL"] = "Netherlands";
    
    // Add major IP ranges for common countries and organizations
    // Note: This is a simplified database. A real implementation would use a complete GeoIP database
    
    // United States ranges (major blocks)
    m_ipRanges.append({ipToInt("8.8.8.0"), ipToInt("8.8.8.255"), "United States", "US", "Google DNS"});
    m_ipRanges.append({ipToInt("1.1.1.0"), ipToInt("1.1.1.255"), "United States", "US", "Cloudflare DNS"});
    m_ipRanges.append({ipToInt("208.67.222.0"), ipToInt("208.67.222.255"), "United States", "US", "OpenDNS"});
    
    // Major US cloud providers
    m_ipRanges.append({ipToInt("54.144.0.0"), ipToInt("54.255.255.255"), "United States", "US", "Amazon AWS"});
    m_ipRanges.append({ipToInt("52.0.0.0"), ipToInt("52.255.255.255"), "United States", "US", "Amazon AWS"});
    m_ipRanges.append({ipToInt("13.104.0.0"), ipToInt("13.107.255.255"), "United States", "US", "Microsoft Azure"});
    m_ipRanges.append({ipToInt("40.64.0.0"), ipToInt("40.127.255.255"), "United States", "US", "Microsoft Azure"});
    m_ipRanges.append({ipToInt("35.184.0.0"), ipToInt("35.191.255.255"), "United States", "US", "Google Cloud"});
    m_ipRanges.append({ipToInt("34.64.0.0"), ipToInt("34.127.255.255"), "United States", "US", "Google Cloud"});
    
    // Facebook/Meta
    m_ipRanges.append({ipToInt("31.13.24.0"), ipToInt("31.13.127.255"), "United States", "US", "Facebook"});
    m_ipRanges.append({ipToInt("69.63.176.0"), ipToInt("69.63.191.255"), "United States", "US", "Facebook"});
    
    // Twitter
    m_ipRanges.append({ipToInt("104.244.42.0"), ipToInt("104.244.43.255"), "United States", "US", "Twitter"});
    
    // Major ISPs
    m_ipRanges.append({ipToInt("71.0.0.0"), ipToInt("71.255.255.255"), "United States", "US", "Comcast"});
    m_ipRanges.append({ipToInt("76.0.0.0"), ipToInt("76.255.255.255"), "United States", "US", "Comcast"});
    m_ipRanges.append({ipToInt("24.0.0.0"), ipToInt("24.255.255.255"), "United States", "US", "Various ISPs"});
    
    // European ranges
    m_ipRanges.append({ipToInt("46.4.0.0"), ipToInt("46.4.255.255"), "Germany", "DE", "Deutsche Telekom"});
    m_ipRanges.append({ipToInt("85.88.0.0"), ipToInt("85.88.255.255"), "Germany", "DE", "Deutsche Telekom"});
    m_ipRanges.append({ipToInt("80.67.0.0"), ipToInt("80.67.255.255"), "United Kingdom", "GB", "BT Group"});
    m_ipRanges.append({ipToInt("90.207.0.0"), ipToInt("90.207.255.255"), "France", "FR", "Orange"});
    
    // Asian ranges
    m_ipRanges.append({ipToInt("61.135.0.0"), ipToInt("61.135.255.255"), "China", "CN", "China Telecom"});
    m_ipRanges.append({ipToInt("119.75.0.0"), ipToInt("119.75.255.255"), "China", "CN", "China Unicom"});
    m_ipRanges.append({ipToInt("210.140.0.0"), ipToInt("210.140.255.255"), "Japan", "JP", "NTT Communications"});
    m_ipRanges.append({ipToInt("203.104.0.0"), ipToInt("203.104.255.255"), "Japan", "JP", "KDDI"});
    
    // Canadian ranges
    m_ipRanges.append({ipToInt("142.177.0.0"), ipToInt("142.177.255.255"), "Canada", "CA", "Bell Canada"});
    m_ipRanges.append({ipToInt("72.139.0.0"), ipToInt("72.139.255.255"), "Canada", "CA", "Rogers Communications"});
    
    // Australian ranges
    m_ipRanges.append({ipToInt("203.12.0.0"), ipToInt("203.12.255.255"), "Australia", "AU", "Telstra"});
    m_ipRanges.append({ipToInt("58.6.0.0"), ipToInt("58.6.255.255"), "Australia", "AU", "Optus"});
    
    // Sort ranges by start IP for binary search
    std::sort(m_ipRanges.begin(), m_ipRanges.end(), 
              [](const IPRange &a, const IPRange &b) {
                  return a.startIP < b.startIP;
              });
}

quint32 IPLookup::ipToInt(const QString &ip) const
{
    QHostAddress addr(ip);
    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
        return addr.toIPv4Address();
    }
    return 0; // Invalid or IPv6
}

bool IPLookup::isPrivateIP(const QString &ip) const
{
    QHostAddress addr(ip);
    
    if (addr.isLoopback()) {
        return true;
    }
    
    quint32 ipInt = addr.toIPv4Address();
    
    // Check private IP ranges
    // 10.0.0.0/8 (10.0.0.0 - 10.255.255.255)
    if (ipInt >= 0x0A000000 && ipInt <= 0x0AFFFFFF) {
        return true;
    }
    
    // 172.16.0.0/12 (172.16.0.0 - 172.31.255.255)
    if (ipInt >= 0xAC100000 && ipInt <= 0xAC1FFFFF) {
        return true;
    }
    
    // 192.168.0.0/16 (192.168.0.0 - 192.168.255.255)
    if (ipInt >= 0xC0A80000 && ipInt <= 0xC0A8FFFF) {
        return true;
    }
    
    // 169.254.0.0/16 (APIPA/Link-local)
    if (ipInt >= 0xA9FE0000 && ipInt <= 0xA9FEFFFF) {
        return true;
    }
    
    return false;
}

QString IPLookup::getWellKnownService(const QString &ip) const
{
    // Map of well-known IP addresses to services
    static const QMap<QString, QString> wellKnownIPs = {
        // Google Services
        {"8.8.8.8", "United States (Google DNS)"},
        {"8.8.4.4", "United States (Google DNS)"},
        {"172.217.0.0", "United States (Google)"},
        
        // Cloudflare
        {"1.1.1.1", "United States (Cloudflare DNS)"},
        {"1.0.0.1", "United States (Cloudflare DNS)"},
        
        // OpenDNS
        {"208.67.222.222", "United States (OpenDNS)"},
        {"208.67.220.220", "United States (OpenDNS)"},
        
        // Microsoft
        {"40.76.4.15", "United States (Microsoft)"},
        {"13.107.42.14", "United States (Microsoft)"},
        
        // Facebook/Meta
        {"31.13.64.35", "United States (Facebook)"},
        {"157.240.0.35", "United States (Facebook)"},
        
        // Twitter
        {"104.244.42.1", "United States (Twitter)"},
        {"104.244.42.129", "United States (Twitter)"},
        
        // Discord
        {"162.159.128.233", "United States (Discord)"},
        {"162.159.130.233", "United States (Discord)"},
        
        // Steam
        {"23.52.74.146", "United States (Steam)"},
        {"184.154.0.69", "United States (Steam)"},
        
        // Netflix
        {"54.230.0.0", "United States (Netflix CDN)"},
        {"52.222.128.0", "United States (Netflix CDN)"},
        
        // YouTube/Google
        {"216.58.194.174", "United States (YouTube)"},
        {"172.217.14.206", "United States (YouTube)"}
    };
    
    if (wellKnownIPs.contains(ip)) {
        return wellKnownIPs[ip];
    }
    
    return QString();
}
