#ifndef ALERTSETTINGSDIALOG_H
#define ALERTSETTINGSDIALOG_H

#include <QDialog>
#include <QPushButton>
#include "alertmanager.h"

namespace Ui {
class AlertSettingsDialog;
}

class AlertSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AlertSettingsDialog(QWidget *parent = nullptr);
    ~AlertSettingsDialog();

    // Getters for the current configuration
    AlertManager::ThresholdConfig thresholdConfig() const;
    bool isAlertTypeEnabled(AlertManager::AlertType type) const;
    
    // Setter for alert manager
    void setAlertManager(AlertManager *alertManager);

private slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
    void restoreDefaults();

private:
    Ui::AlertSettingsDialog *ui;
    
    void loadSettings();
    void saveSettings();
    void setupConnections();
    void setupUi();
};

#endif // ALERTSETTINGSDIALOG_H
