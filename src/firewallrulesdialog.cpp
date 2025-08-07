#include "firewallrulesdialog.h"
#include "ui_firewallrulesdialog.h"

#include <QMessageBox>
#include <QMenu>
#include <QInputDialog>
#include <QFileDialog>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QDateTime>
#include <QLocale>

FirewallRulesDialog::FirewallRulesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::FirewallRulesDialog),
    m_firewallManager(FirewallManager::instance())
{
    ui->setupUi(this);
    setWindowTitle(tr("Firewall Rules"));
    
    setupUI();
    setupConnections();
    
    // Load existing rules
    updateRulesList();
}

FirewallRulesDialog::~FirewallRulesDialog()
{
    delete ui;
}

void FirewallRulesDialog::setupUI()
{
    // Set up the rules table
    QStandardItemModel *model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({
        tr("Name"),
        tr("Type"),
        tr("Application/Address"),
        tr("Protocol"),
        tr("Port"),
        tr("Status"),
        tr("Created")
    });
    
    ui->rulesTable->setModel(model);
    ui->rulesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->rulesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->rulesTable->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->rulesTable->horizontalHeader()->setStretchLastSection(true);
    
    // Set up buttons
    ui->addButton->setIcon(QIcon(":/png/add.png"));
    ui->removeButton->setIcon(QIcon(":/png/delete.png"));
    ui->enableButton->setCheckable(true);
    
    // Disable buttons until a rule is selected
    ui->removeButton->setEnabled(false);
    ui->enableButton->setEnabled(false);
}

void FirewallRulesDialog::setupConnections()
{
    // Connect buttons
    connect(ui->addButton, &QPushButton::clicked, this, &FirewallRulesDialog::onAddRule);
    connect(ui->removeButton, &QPushButton::clicked, this, &FirewallRulesDialog::onRemoveRule);
    connect(ui->enableButton, &QPushButton::toggled, this, &FirewallRulesDialog::onEnableRule);
    
    // Connect table signals
    connect(ui->rulesTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &FirewallRulesDialog::onRuleSelectionChanged);
    connect(ui->rulesTable, &QTableView::doubleClicked,
            this, &FirewallRulesDialog::onRuleDoubleClicked);
    
    // Connect firewall manager signals
    connect(m_firewallManager, &FirewallManager::ruleAdded,
            this, &FirewallRulesDialog::onRuleAdded);
    connect(m_firewallManager, &FirewallManager::ruleRemoved,
            this, &FirewallRulesDialog::onRuleRemoved);
    connect(m_firewallManager, &FirewallManager::ruleUpdated,
            this, &FirewallRulesDialog::onRuleUpdated);
    connect(m_firewallManager, &FirewallManager::errorOccurred,
            this, &FirewallRulesDialog::onErrorOccurred);
}

void FirewallRulesDialog::updateRulesList()
{
    QStandardItemModel *model = qobject_cast<QStandardItemModel*>(ui->rulesTable->model());
    if (!model) return;
    
    model->removeRows(0, model->rowCount());
    m_rules.clear();
    const auto rules = m_firewallManager->rules();
    for (const auto &rule : rules) {
        m_rules[rule.id] = rule;
        
        QString typeStr;
        switch (rule.type) {
        case FirewallManager::BlockApp: typeStr = tr("Block App"); break;
        case FirewallManager::AllowApp: typeStr = tr("Allow App"); break;
        case FirewallManager::BlockInbound: typeStr = tr("Block Inbound"); break;
        case FirewallManager::BlockOutbound: typeStr = tr("Block Outbound"); break;
        case FirewallManager::BlockAll: typeStr = tr("Block All"); break;
        }
        
        QString target;
        if (rule.type == FirewallManager::BlockApp || rule.type == FirewallManager::AllowApp) {
            target = QFileInfo(rule.appPath).fileName();
        } else {
            target = rule.remoteAddress;
            if (!rule.remotePort.isEmpty()) {
                target += ":" + rule.remotePort;
            }
        }
        
        QString protocolStr;
        switch (rule.protocol) {
        case FirewallManager::TCP: protocolStr = "TCP"; break;
        case FirewallManager::UDP: protocolStr = "UDP"; break;
        case FirewallManager::ICMP: protocolStr = "ICMP"; break;
        default: protocolStr = tr("Any");
        }
        
        QString portStr;
        if (!rule.localPort.isEmpty()) {
            portStr = rule.localPort;
        } else if (!rule.remotePort.isEmpty()) {
            portStr = rule.remotePort;
        }
        
        QList<QStandardItem*> items = {
            new QStandardItem(rule.name),
            new QStandardItem(typeStr),
            new QStandardItem(target),
            new QStandardItem(protocolStr),
            new QStandardItem(portStr),
            new QStandardItem(rule.enabled ? tr("Enabled") : tr("Disabled")),
            new QStandardItem(rule.created.toString(QLocale::system().dateFormat(QLocale::ShortFormat)))
        };
        
        // Store rule ID in the first item's data
        items.first()->setData(rule.id, Qt::UserRole);
        
        // Add row to the model
        model->appendRow(items);
    }
    
    // Resize columns to fit content
    ui->rulesTable->resizeColumnsToContents();
}

void FirewallRulesDialog::showRuleDetails(const FirewallManager::FirewallRule &rule)
{
    QString details;
    details += QString("<b>%1</b><br>").arg(rule.name);
    details += QString("<b>%1:</b> %2<br>").arg(tr("Type")).arg([rule]() {
        switch (rule.type) {
        case FirewallManager::BlockApp: return tr("Block Application");
        case FirewallManager::AllowApp: return tr("Allow Application");
        case FirewallManager::BlockInbound: return tr("Block Inbound");
        case FirewallManager::BlockOutbound: return tr("Block Outbound");
        case FirewallManager::BlockAll: return tr("Block All Traffic");
        default: return tr("Unknown");
        }
    }());
    
    if (!rule.appPath.isEmpty()) {
        details += QString("<b>%1:</b> %2<br>").arg(tr("Application")).arg(rule.appPath);
    }
    
    if (!rule.remoteAddress.isEmpty()) {
        details += QString("<b>%1:</b> %2<br>").arg(tr("Remote Address")).arg(rule.remoteAddress);
    }
    
    if (!rule.localPort.isEmpty() || !rule.remotePort.isEmpty()) {
        if (!rule.localPort.isEmpty()) {
            details += QString("<b>%1:</b> %2<br>").arg(tr("Local Port")).arg(rule.localPort);
        }
        if (!rule.remotePort.isEmpty()) {
            details += QString("<b>%1:</b> %2<br>").arg(tr("Remote Port")).arg(rule.remotePort);
        }
    }
    
    details += QString("<b>%1:</b> %2<br>").arg(tr("Protocol")).arg([rule]() -> QString {
        switch (rule.protocol) {
        case FirewallManager::TCP: return "TCP";
        case FirewallManager::UDP: return "UDP";
        case FirewallManager::ICMP: return "ICMP";
        default: return tr("Any");
        }
    }());
    
    details += QString("<b>%1:</b> %2<br>").arg(tr("Status"))
              .arg(rule.enabled ? tr("Enabled") : tr("Disabled"));
    
    details += QString("<b>%1:</b> %2<br>").arg(tr("Created"))
              .arg(rule.created.toString(QLocale::system().dateFormat(QLocale::LongFormat)));
    
    QMessageBox::information(this, tr("Rule Details"), details);
}

void FirewallRulesDialog::onAddRule()
{
    // Create a menu for rule types
    QMenu menu(this);
    
    QAction *blockAppAction = menu.addAction(tr("Block Application"));
    QAction *allowAppAction = menu.addAction(tr("Allow Application"));
    menu.addSeparator();
    QAction *blockInAction = menu.addAction(tr("Block Inbound Connection"));
    QAction *blockOutAction = menu.addAction(tr("Block Outbound Connection"));
    
    // Show the menu and get the selected action
    QAction *selectedAction = menu.exec(QCursor::pos());
    if (!selectedAction) return;
    
    FirewallManager::FirewallRule rule;
    
    if (selectedAction == blockAppAction || selectedAction == allowAppAction) {
        // Application rule
        QString appPath = QFileDialog::getOpenFileName(this, tr("Select Application"));
        if (appPath.isEmpty()) return;
        
        rule.type = (selectedAction == blockAppAction) ? 
            FirewallManager::BlockApp : FirewallManager::AllowApp;
        rule.appPath = QFileInfo(appPath).canonicalFilePath();
        rule.name = QFileInfo(appPath).fileName();
    } 
    else if (selectedAction == blockInAction || selectedAction == blockOutAction) {
        // Network rule
        bool ok;
        QString address = QInputDialog::getText(this, tr("Block Connection"),
            tr("Enter IP address or hostname (leave empty for any):"), 
            QLineEdit::Normal, "", &ok);
        
        if (!ok) return;
        
        QString port = QInputDialog::getText(this, tr("Block Connection"),
            tr("Enter port number (leave empty for all ports):"), 
            QLineEdit::Normal, "", &ok);
        
        if (!ok) return;
        
        rule.type = (selectedAction == blockInAction) ? 
            FirewallManager::BlockInbound : FirewallManager::BlockOutbound;
        rule.remoteAddress = address;
        rule.remotePort = port;
        
        // Set protocol
        QStringList protocols = {tr("Any"), "TCP", "UDP", "ICMP"};
        QString protocol = QInputDialog::getItem(this, tr("Select Protocol"),
            tr("Protocol:"), protocols, 0, false, &ok);
        
        if (!ok) return;
        
        if (protocol == "TCP") rule.protocol = FirewallManager::TCP;
        else if (protocol == "UDP") rule.protocol = FirewallManager::UDP;
        else if (protocol == "ICMP") rule.protocol = FirewallManager::ICMP;
        else rule.protocol = FirewallManager::Any;
        
        // Generate a name if not provided
        rule.name = tr("%1 %2%3%4")
            .arg(selectedAction == blockInAction ? "Block Inbound" : "Block Outbound")
            .arg(address.isEmpty() ? "Any" : address)
            .arg(port.isEmpty() ? "" : ":" + port)
            .arg(protocol == "Any" ? "" : " (" + protocol + ")");
    }
    
    // Add the rule
    m_firewallManager->addRule(rule);
}

void FirewallRulesDialog::onRemoveRule()
{
    QModelIndexList selected = ui->rulesTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;
    
    QString ruleId = selected.first().data(Qt::UserRole).toString();
    if (ruleId.isEmpty()) return;
    
    int ret = QMessageBox::question(this, tr("Remove Rule"),
        tr("Are you sure you want to remove the selected rule?"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret == QMessageBox::Yes) {
        m_firewallManager->removeRule(ruleId);
    }
}

void FirewallRulesDialog::onEnableRule(bool enabled)
{
    QModelIndexList selected = ui->rulesTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;
    
    QString ruleId = selected.first().data(Qt::UserRole).toString();
    if (ruleId.isEmpty()) return;
    
    m_firewallManager->enableRule(ruleId, enabled);
}

void FirewallRulesDialog::onRuleSelectionChanged()
{
    QModelIndexList selected = ui->rulesTable->selectionModel()->selectedRows();
    bool hasSelection = !selected.isEmpty();
    
    ui->removeButton->setEnabled(hasSelection);
    ui->enableButton->setEnabled(hasSelection);
    
    if (hasSelection) {
        QString ruleId = selected.first().data(Qt::UserRole).toString();
        if (m_rules.contains(ruleId)) {
            ui->enableButton->setChecked(m_rules[ruleId].enabled);
        }
    }
}

void FirewallRulesDialog::onRuleDoubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) return;
    
    QString ruleId = index.sibling(index.row(), 0).data(Qt::UserRole).toString();
    if (m_rules.contains(ruleId)) {
        showRuleDetails(m_rules[ruleId]);
    }
}

void FirewallRulesDialog::onRuleAdded(const FirewallManager::FirewallRule &rule)
{
    Q_UNUSED(rule);
    updateRulesList();
}

void FirewallRulesDialog::onRuleRemoved(const QString &ruleId)
{
    Q_UNUSED(ruleId);
    updateRulesList();
}

void FirewallRulesDialog::onRuleUpdated(const FirewallManager::FirewallRule &rule)
{
    Q_UNUSED(rule);
    updateRulesList();
}

void FirewallRulesDialog::onErrorOccurred(const QString &error)
{
    QMessageBox::warning(this, tr("Firewall Error"), error);
}
