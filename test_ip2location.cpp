#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include "src/ip2location.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    qDebug() << "Testing IP2Location DB11LITEBINIPV6 implementation...";
    
    IP2Location ip2location;
    
    // Test IPv4 addresses
    QStringList testIPv4 = {
        "8.8.8.8",      // Google DNS
        "1.1.1.1",      // Cloudflare DNS
        "208.67.222.222", // OpenDNS
        "192.168.1.1",  // Private IP
        "127.0.0.1"     // Loopback
    };
    
    // Test IPv6 addresses
    QStringList testIPv6 = {
        "2001:4860:4860::8888",  // Google DNS IPv6
        "2606:4700:4700::1111",  // Cloudflare DNS IPv6
        "2620:119:35::35",       // OpenDNS IPv6
        "fe80::1",               // Link-local
        "::1"                    // IPv6 loopback
    };
    
    qDebug() << "\n=== Testing IPv4 addresses ===";
    for (const QString &ip : testIPv4) {
        QHostAddress addr(ip);
        qDebug() << "Testing IP:" << ip << "(Protocol:" << addr.protocol() << ")";
        
        IP2Location::LocationInfo location = ip2location.getLocationFromIP(ip);
        qDebug() << "  Country:" << location.country;
        qDebug() << "  Region:" << location.region;
        qDebug() << "  City:" << location.city;
        qDebug() << "  ISP:" << location.isp;
        qDebug() << "  Coordinates:" << location.latitude << "," << location.longitude;
        qDebug() << "  Display:" << location.toDisplayString();
        qDebug() << "  Detailed:" << location.toDetailedString();
        qDebug() << "";
    }
    
    qDebug() << "\n=== Testing IPv6 addresses ===";
    for (const QString &ip : testIPv6) {
        QHostAddress addr(ip);
        qDebug() << "Testing IP:" << ip << "(Protocol:" << addr.protocol() << ")";
        
        IP2Location::LocationInfo location = ip2location.getLocationFromIP(ip);
        qDebug() << "  Country:" << location.country;
        qDebug() << "  Region:" << location.region;
        qDebug() << "  City:" << location.city;
        qDebug() << "  ISP:" << location.isp;
        qDebug() << "  Coordinates:" << location.latitude << "," << location.longitude;
        qDebug() << "  Display:" << location.toDisplayString();
        qDebug() << "  Detailed:" << location.toDetailedString();
        qDebug() << "";
    }
    
    qDebug() << "\n=== Database Status ===";
    qDebug() << "Database ready:" << ip2location.isDatabaseReady();
    qDebug() << "Database info:" << ip2location.getDatabaseInfo();
    qDebug() << "Database path:" << ip2location.getDatabasePath();
    
    qDebug() << "\n=== Testing Database Download ===";
    qDebug() << "Starting database download...";
    
    QObject::connect(&ip2location, &IP2Location::databaseDownloadStarted, []() {
        qDebug() << "Database download started";
    });
    
    QObject::connect(&ip2location, &IP2Location::databaseDownloadProgress, [](qint64 received, qint64 total) {
        if (total > 0) {
            int percent = (received * 100) / total;
            qDebug() << "Download progress:" << percent << "% (" << received << "/" << total << "bytes)";
        }
    });
    
    QObject::connect(&ip2location, &IP2Location::databaseDownloadFinished, [&ip2location](bool success) {
        qDebug() << "Database download finished, success:" << success;
        if (success) {
            qDebug() << "Database loaded successfully!";
            qDebug() << "Database info:" << ip2location.getDatabaseInfo();
            
            // Test a few IPs after download
            QStringList testIPs = {"8.8.8.8", "2001:4860:4860::8888"};
            for (const QString &ip : testIPs) {
                IP2Location::LocationInfo location = ip2location.getLocationFromIP(ip);
                qDebug() << "Test IP" << ip << "->" << location.toDisplayString();
            }
        }
        
        QCoreApplication::quit();
    });
    
    ip2location.downloadDatabase();
    
    return app.exec();
}
