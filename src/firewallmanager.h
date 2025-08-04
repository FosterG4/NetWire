#ifndef FIREWALLMANAGER_H
#define FIREWALLMANAGER_H

#include <QObject>
#include <QString>
#include <QSet>
#include <QMap>
#include <QProcess>
#include <QThread>
#include <QMutex>
#include <QDateTime>

class FirewallManager : public QObject
{
    Q_OBJECT
    
public:
    enum RuleType {
        BlockAll,
        BlockInbound,
        BlockOutbound,
        AllowApp,
        BlockApp
    };
    
    enum Protocol {
        Any,
        TCP,
        UDP,
        ICMP
    };
    
    struct FirewallRule {
        QString id;
        QString name;
        QString appPath;
        RuleType type;
        Protocol protocol;
        QString localPort;
        QString remoteAddress;
        QString remotePort;
        bool enabled;
        QDateTime created;
    };

    static FirewallManager* instance();
    ~FirewallManager();

    bool isAppBlocked(const QString &appPath) const;
    bool isAddressBlocked(const QString &address) const;
    bool isPortBlocked(quint16 port, Protocol protocol = Any) const;
    
    QString addRule(const FirewallRule &rule);
    bool removeRule(const QString &ruleId);
    bool enableRule(const QString &ruleId, bool enable = true);
    QList<FirewallRule> rules() const;
    
    // Convenience methods
    QString blockApplication(const QString &appPath, const QString &appName = QString());
    QString blockAddress(const QString &address, const QString &name = QString(), Protocol protocol = Any);
    QString blockPort(quint16 port, const QString &name = QString(), Protocol protocol = Any);
    
    bool isFirewallEnabled() const;
    bool setFirewallEnabled(bool enabled);
    
signals:
    void ruleAdded(const FirewallManager::FirewallRule &rule);
    void ruleRemoved(const QString &ruleId);
    void ruleUpdated(const FirewallManager::FirewallRule &rule);
    void firewallStateChanged(bool enabled);
    void errorOccurred(const QString &error);

private:
    explicit FirewallManager(QObject *parent = nullptr);
    
    void loadRules();
    void saveRules();
    QString generateRuleId() const;
    bool applyWindowsFirewallRule(const FirewallRule &rule, bool remove = false);
    
    static FirewallManager *m_instance;
    QMap<QString, FirewallRule> m_rules;
    mutable QMutex m_mutex;
    QString m_rulesFilePath;
};

#endif // FIREWALLMANAGER_H
