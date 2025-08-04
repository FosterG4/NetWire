#include "firewallmanager.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>
#include <QProcess>
#include <QDebug>

FirewallManager* FirewallManager::m_instance = nullptr;

FirewallManager::FirewallManager(QObject *parent)
    : QObject(parent)
{
    // Set up rules file path
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir appDataDir(appDataPath);
    if (!appDataDir.exists()) {
        appDataDir.mkpath(".");
    }
    m_rulesFilePath = appDataPath + "/firewall_rules.json";
    
    // Load existing rules
    loadRules();
}

FirewallManager::~FirewallManager()
{
    saveRules();
}

FirewallManager* FirewallManager::instance()
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    if (!m_instance) {
        m_instance = new FirewallManager(qApp);
    }
    return m_instance;
}

bool FirewallManager::isAppBlocked(const QString &appPath) const
{
    QMutexLocker locker(&m_mutex);
    const QString canonicalPath = QFileInfo(appPath).canonicalFilePath();
    
    for (const FirewallRule &rule : m_rules) {
        if ((rule.type == BlockApp || rule.type == AllowApp) && 
            QFileInfo(rule.appPath).canonicalFilePath() == canonicalPath) {
            return rule.enabled && rule.type == BlockApp;
        }
    }
    return false;
}

bool FirewallManager::isAddressBlocked(const QString &address) const
{
    QMutexLocker locker(&m_mutex);
    for (const FirewallRule &rule : m_rules) {
        if ((rule.type == BlockInbound || rule.type == BlockOutbound) && 
            rule.remoteAddress == address && rule.enabled) {
            return true;
        }
    }
    return false;
}

bool FirewallManager::isPortBlocked(quint16 port, Protocol protocol) const
{
    QMutexLocker locker(&m_mutex);
    const QString portStr = QString::number(port);
    
    for (const FirewallRule &rule : m_rules) {
        if (!rule.enabled) continue;
        
        if ((rule.protocol == protocol || rule.protocol == Any) && 
            (rule.localPort == portStr || rule.remotePort == portStr)) {
            return true;
        }
    }
    return false;
}

QString FirewallManager::addRule(const FirewallRule &rule)
{
    FirewallRule newRule = rule;
    if (newRule.id.isEmpty()) {
        newRule.id = generateRuleId();
    }
    
    if (newRule.name.isEmpty()) {
        // Generate a default name based on rule type
        switch (newRule.type) {
        case BlockApp: 
            newRule.name = tr("Block %1").arg(QFileInfo(newRule.appPath).fileName()); 
            break;
        case AllowApp: 
            newRule.name = tr("Allow %1").arg(QFileInfo(newRule.appPath).fileName()); 
            break;
        case BlockInbound: 
            newRule.name = tr("Block Inbound %1").arg(newRule.remoteAddress); 
            break;
        case BlockOutbound: 
            newRule.name = tr("Block Outbound %1").arg(newRule.remoteAddress); 
            break;
        case BlockAll: 
            newRule.name = tr("Block All Traffic"); 
            break;
        }
    }
    
    newRule.created = QDateTime::currentDateTime();
    
    // Apply the rule to Windows Firewall
    if (!applyWindowsFirewallRule(newRule)) {
        emit errorOccurred(tr("Failed to apply firewall rule"));
        return QString();
    }
    
    {
        QMutexLocker locker(&m_mutex);
        m_rules[newRule.id] = newRule;
    }
    
    saveRules();
    emit ruleAdded(newRule);
    return newRule.id;
}

bool FirewallManager::removeRule(const QString &ruleId)
{
    FirewallRule rule;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_rules.contains(ruleId)) {
            return false;
        }
        rule = m_rules[ruleId];
    }
    
    // Remove the rule from Windows Firewall
    if (!applyWindowsFirewallRule(rule, true)) {
        emit errorOccurred(tr("Failed to remove firewall rule"));
        return false;
    }
    
    {
        QMutexLocker locker(&m_mutex);
        m_rules.remove(ruleId);
    }
    
    saveRules();
    emit ruleRemoved(ruleId);
    return true;
}

bool FirewallManager::enableRule(const QString &ruleId, bool enable)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_rules.contains(ruleId)) {
        return false;
    }
    
    if (m_rules[ruleId].enabled == enable) {
        return true; // No change needed
    }
    
    FirewallRule rule = m_rules[ruleId];
    rule.enabled = enable;
    
    // Update Windows Firewall
    if (!applyWindowsFirewallRule(rule, !enable)) {
        emit errorOccurred(tr("Failed to update firewall rule"));
        return false;
    }
    
    m_rules[ruleId] = rule;
    saveRules();
    emit ruleUpdated(rule);
    return true;
}

QList<FirewallManager::FirewallRule> FirewallManager::rules() const
{
    QMutexLocker locker(&m_mutex);
    return m_rules.values();
}

QString FirewallManager::blockApplication(const QString &appPath, const QString &appName)
{
    FirewallRule rule;
    rule.type = BlockApp;
    rule.appPath = QFileInfo(appPath).canonicalFilePath();
    rule.name = appName.isEmpty() ? QFileInfo(appPath).fileName() : appName;
    rule.protocol = Any;
    rule.enabled = true;
    
    return addRule(rule);
}

QString FirewallManager::blockAddress(const QString &address, const QString &name, Protocol protocol)
{
    FirewallRule rule;
    rule.type = BlockOutbound;
    rule.remoteAddress = address;
    rule.name = name.isEmpty() ? tr("Block %1").arg(address) : name;
    rule.protocol = protocol;
    rule.enabled = true;
    
    return addRule(rule);
}

QString FirewallManager::blockPort(quint16 port, const QString &name, Protocol protocol)
{
    FirewallRule rule;
    rule.type = BlockInbound;
    rule.localPort = QString::number(port);
    rule.name = name.isEmpty() ? tr("Block Port %1").arg(port) : name;
    rule.protocol = protocol;
    rule.enabled = true;
    
    return addRule(rule);
}

bool FirewallManager::isFirewallEnabled() const
{
    // Check if Windows Firewall is enabled
    QProcess process;
    process.start("netsh", QStringList() << "advfirewall" << "show" << "allprofiles" << "state");
    if (!process.waitForFinished(3000)) {
        return false;
    }
    
    QString output = QString::fromLocal8Bit(process.readAllStandardOutput());
    return output.contains("ON", Qt::CaseInsensitive);
}

bool FirewallManager::setFirewallEnabled(bool enabled)
{
    QString state = enabled ? "on" : "off";
    
    // Enable/disable Windows Firewall for all profiles
    QProcess process;
    process.start("netsh", QStringList() << "advfirewall" << "set" << "allprofiles" << "state" << state);
    if (!process.waitForFinished(3000) || process.exitCode() != 0) {
        emit errorOccurred(tr("Failed to %1 firewall").arg(enabled ? "enable" : "disable"));
        return false;
    }
    
    emit firewallStateChanged(enabled);
    return true;
}

void FirewallManager::loadRules()
{
    QFile file(m_rulesFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "No existing firewall rules file found, starting with empty rules";
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isArray()) {
        qWarning() << "Invalid firewall rules file format";
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    m_rules.clear();
    
    for (const QJsonValue &value : doc.array()) {
        QJsonObject obj = value.toObject();
        FirewallRule rule;
        
        rule.id = obj["id"].toString();
        rule.name = obj["name"].toString();
        rule.appPath = obj["appPath"].toString();
        rule.type = static_cast<RuleType>(obj["type"].toInt());
        rule.protocol = static_cast<Protocol>(obj["protocol"].toInt());
        rule.localPort = obj["localPort"].toString();
        rule.remoteAddress = obj["remoteAddress"].toString();
        rule.remotePort = obj["remotePort"].toString();
        rule.enabled = obj["enabled"].toBool();
        rule.created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
        
        if (!rule.id.isEmpty()) {
            m_rules[rule.id] = rule;
        }
    }
    
    qDebug() << "Loaded" << m_rules.size() << "firewall rules";
}

void FirewallManager::saveRules()
{
    QMutexLocker locker(&m_mutex);
    
    QJsonArray rulesArray;
    for (const FirewallRule &rule : m_rules) {
        QJsonObject obj;
        obj["id"] = rule.id;
        obj["name"] = rule.name;
        obj["appPath"] = rule.appPath;
        obj["type"] = static_cast<int>(rule.type);
        obj["protocol"] = static_cast<int>(rule.protocol);
        obj["localPort"] = rule.localPort;
        obj["remoteAddress"] = rule.remoteAddress;
        obj["remotePort"] = rule.remotePort;
        obj["enabled"] = rule.enabled;
        obj["created"] = rule.created.toString(Qt::ISODate);
        
        rulesArray.append(obj);
    }
    
    QFile file(m_rulesFilePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to save firewall rules:" << file.errorString();
        return;
    }
    
    file.write(QJsonDocument(rulesArray).toJson());
}

QString FirewallManager::generateRuleId() const
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

bool FirewallManager::applyWindowsFirewallRule(const FirewallRule &rule, bool remove)
{
    QStringList args;
    args << "advfirewall" << "firewall";
    
    if (remove) {
        args << "delete" << "rule";
        args << "name=" + rule.name;
    } else {
        args << "add" << "rule";
        args << "name=" + rule.name;
        
        // Set direction and action
        switch (rule.type) {
        case BlockInbound:
            args << "dir=in" << "action=block";
            break;
        case BlockOutbound:
        case BlockApp:
            args << "dir=out" << "action=block";
            break;
        case AllowApp:
            args << "dir=out" << "action=allow";
            break;
        case BlockAll:
            args << "dir=in" << "action=block";
            break;
        }
        
        // Set program path for application rules
        if (rule.type == BlockApp || rule.type == AllowApp) {
            args << "program=\"" + QDir::toNativeSeparators(rule.appPath) + "\"";
        }
        
        // Set protocol
        switch (rule.protocol) {
        case TCP:
            args << "protocol=TCP";
            break;
        case UDP:
            args << "protocol=UDP";
            break;
        case ICMP:
            args << "protocol=ICMPv4";
            break;
        case Any:
        default:
            args << "protocol=any";
            break;
        }
        
        // Set local port
        if (!rule.localPort.isEmpty()) {
            args << "localport=" + rule.localPort;
        }
        
        // Set remote address and port
        if (!rule.remoteAddress.isEmpty()) {
            args << "remoteip=" + rule.remoteAddress;
        }
        if (!rule.remotePort.isEmpty()) {
            args << "remoteport=" + rule.remotePort;
        }
        
        // Enable the rule
        args << "enable=yes";
    }
    
    // Execute the command
    QProcess process;
    process.start("netsh", args);
    
    if (!process.waitForFinished(5000)) {
        qWarning() << "Failed to execute netsh command:" << process.errorString();
        return false;
    }
    
    if (process.exitCode() != 0) {
        qWarning() << "Failed to apply firewall rule:" << process.readAllStandardError();
        return false;
    }
    
    return true;
}
