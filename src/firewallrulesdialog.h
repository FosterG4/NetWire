#ifndef FIREWALLRULESDIALOG_H
#define FIREWALLRULESDIALOG_H

#include <QDialog>
#include "firewallmanager.h"

namespace Ui {
class FirewallRulesDialog;
}

class FirewallRulesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FirewallRulesDialog(QWidget *parent = nullptr);
    ~FirewallRulesDialog();

private slots:
    void onAddRule();
    void onRemoveRule();
    void onEnableRule(bool enabled);
    void onRuleSelectionChanged();
    void onRuleDoubleClicked(const QModelIndex &index);
    
    void onRuleAdded(const FirewallManager::FirewallRule &rule);
    void onRuleRemoved(const QString &ruleId);
    void onRuleUpdated(const FirewallManager::FirewallRule &rule);
    void onErrorOccurred(const QString &error);

private:
    void setupUI();
    void setupConnections();
    void updateRulesList();
    void showRuleDetails(const FirewallManager::FirewallRule &rule);
    
    Ui::FirewallRulesDialog *ui;
    FirewallManager *m_firewallManager;
    QMap<QString, FirewallManager::FirewallRule> m_rules;
};

#endif // FIREWALLRULESDIALOG_H
