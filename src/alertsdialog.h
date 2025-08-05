#ifndef ALERTSDIALOG_H
#define ALERTSDIALOG_H

#include <QDialog>
#include <QStandardItemModel>
#include <QLabel>
#include <QGroupBox>
#include <QTextEdit>
#include <QPushButton>
#include "alertmanager.h"

namespace Ui {
class AlertsDialog;
}

class AlertsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AlertsDialog(QWidget *parent = nullptr);
    ~AlertsDialog();

    void setAlertManager(AlertManager *manager);
    void updateAlerts();

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onNewAlert(const AlertManager::Alert &alert);
    void onAlertAcknowledged(int alertId);
    void onAlertCleared(int alertId);
    void onThresholdReached(AlertManager::AlertType type, const QString &message);
    
    void on_tabWidget_currentChanged(int index);
    void on_btnAcknowledge_clicked();
    void on_btnClear_clicked();
    void on_btnClearAll_clicked();
    void on_btnSettings_clicked();
    void on_tblAlerts_doubleClicked(const QModelIndex &index);
    void on_txtFilter_textChanged(const QString &text);
    
    void showContextMenu(const QPoint &pos);
    void acknowledgeSelected();
    void clearSelected();
    void showAlertDetails();
    
private:
    Ui::AlertsDialog *ui;
    AlertManager *m_alertManager;
    QStandardItemModel *m_activeAlertsModel;
    QStandardItemModel *m_alertHistoryModel;
    QHash<int, AlertManager::Alert> m_alertMap;
    
    void setupUi();
    void setupConnections();
    void updateActiveAlerts();
    void updateAlertHistory();
    void applyFilter(const QString &filterText);
    void showAlertDetails(const AlertManager::Alert &alert);
    QList<QStandardItem*> createAlertRow(const AlertManager::Alert &alert) const;
    void addAlertToModel(const AlertManager::Alert &alert, QStandardItemModel *model);
    void updateAlertInModel(const AlertManager::Alert &alert, QStandardItemModel *model);
    void removeAlertFromModel(int alertId, QStandardItemModel *model);
    
    // Helper methods for alert display
    static QString alertTypeToString(AlertManager::AlertType type);
    static QString alertSeverityToString(AlertManager::Severity severity);
    static QString formatBytes(qint64 bytes);
    
    // Column indices
    enum AlertColumns {
        ColId,
        ColSeverity,
        ColType,
        ColTitle,
        ColSource,
        ColDestination,
        ColTimestamp,
        ColAcknowledged,
        ColAlertData, // Hidden column to store the full alert object
        ColCount
    };
};

#endif // ALERTSDIALOG_H
