#include "alertsettingsdialog.h"
#include "ui_alertsettingsdialog.h"
#include "alertmanager.h"
#include <QSettings>
#include <QMessageBox>

AlertSettingsDialog::AlertSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AlertSettingsDialog)
{
    ui->setupUi(this);
    
    // Set window flags to remove the help button
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    // Set up the UI
    setupUi();
    
    // Load saved settings
    loadSettings();
    
    // Connect signals and slots
    setupConnections();
}

AlertSettingsDialog::~AlertSettingsDialog()
{
    delete ui;
}

AlertManager::ThresholdConfig AlertSettingsDialog::thresholdConfig() const
{
    AlertManager::ThresholdConfig config;
    
    // Get values from UI
    config.bandwidthThreshold = ui->spinBandwidthThreshold->value();
    config.connectionSpikeWindow = ui->spinConnectionWindow->value();
    config.connectionSpikeCount = ui->spinConnectionThreshold->value();
    config.dataExfiltrationKB = ui->spinDataExfiltration->value();
    
    return config;
}

bool AlertSettingsDialog::isAlertTypeEnabled(AlertManager::AlertType type) const
{
    switch (type) {
    case AlertManager::NewAppDetected:
        return ui->chkNewApp->isChecked();
    case AlertManager::HighBandwidthUsage:
        return ui->chkHighBandwidth->isChecked();
    case AlertManager::SuspiciousConnection:
        return ui->chkSuspiciousConn->isChecked();
    case AlertManager::PortScanDetected:
        return ui->chkPortScan->isChecked();
    case AlertManager::DataExfiltration:
        return ui->chkDataExfiltration->isChecked();
    case AlertManager::ProtocolAnomaly:
        return ui->chkProtocolAnomaly->isChecked();
    case AlertManager::ConnectionSpike:
        return ui->chkConnectionSpike->isChecked();
    case AlertManager::RuleViolation:
        return ui->chkRuleViolation->isChecked();
    case AlertManager::CustomAlert:
        return ui->chkCustomAlert->isChecked();
    default:
        return false;
    }
}

void AlertSettingsDialog::on_buttonBox_accepted()
{
    // Save settings before accepting
    saveSettings();
    accept();
}

void AlertSettingsDialog::on_buttonBox_rejected()
{
    // Just close the dialog without saving
    reject();
}

void AlertSettingsDialog::restoreDefaults()
{
    // Restore default values
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Restore Defaults"),
                                tr("Are you sure you want to restore all alert settings to their default values?"),
                                QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        // Set default threshold values
        ui->spinBandwidthThreshold->setValue(1024); // 1 MB/s
        ui->spinConnectionWindow->setValue(60); // 1 minute
        ui->spinConnectionThreshold->setValue(50); // 50 connections
        ui->spinDataExfiltration->setValue(10 * 1024); // 10 MB
        
        // Enable all alert types by default
        ui->chkNewApp->setChecked(true);
        ui->chkHighBandwidth->setChecked(true);
        ui->chkSuspiciousConn->setChecked(true);
        ui->chkPortScan->setChecked(true);
        ui->chkDataExfiltration->setChecked(true);
        ui->chkProtocolAnomaly->setChecked(true);
        ui->chkConnectionSpike->setChecked(true);
        ui->chkRuleViolation->setChecked(true);
        ui->chkCustomAlert->setChecked(true);
    }
}

void AlertSettingsDialog::loadSettings()
{
    QSettings settings("NetWire", "NetWire");
    
    // Load threshold settings
    settings.beginGroup("AlertThresholds");
    ui->spinBandwidthThreshold->setValue(settings.value("bandwidthThreshold", 1024).toInt());
    ui->spinConnectionWindow->setValue(settings.value("connectionSpikeWindow", 60).toInt());
    ui->spinConnectionThreshold->setValue(settings.value("connectionSpikeCount", 50).toInt());
    ui->spinDataExfiltration->setValue(settings.value("dataExfiltrationKB", 10 * 1024).toInt());
    settings.endGroup();
    
    // Load alert type settings
    settings.beginGroup("EnabledAlerts");
    ui->chkNewApp->setChecked(settings.value("AlertType_0", true).toBool());
    ui->chkHighBandwidth->setChecked(settings.value("AlertType_1", true).toBool());
    ui->chkSuspiciousConn->setChecked(settings.value("AlertType_2", true).toBool());
    ui->chkPortScan->setChecked(settings.value("AlertType_3", true).toBool());
    ui->chkDataExfiltration->setChecked(settings.value("AlertType_4", true).toBool());
    ui->chkProtocolAnomaly->setChecked(settings.value("AlertType_5", true).toBool());
    ui->chkConnectionSpike->setChecked(settings.value("AlertType_6", true).toBool());
    ui->chkRuleViolation->setChecked(settings.value("AlertType_7", true).toBool());
    ui->chkCustomAlert->setChecked(settings.value("AlertType_8", true).toBool());
    settings.endGroup();
}

void AlertSettingsDialog::saveSettings()
{
    QSettings settings("NetWire", "NetWire");
    
    // Save threshold settings
    settings.beginGroup("AlertThresholds");
    settings.setValue("bandwidthThreshold", ui->spinBandwidthThreshold->value());
    settings.setValue("connectionSpikeWindow", ui->spinConnectionWindow->value());
    settings.setValue("connectionSpikeCount", ui->spinConnectionThreshold->value());
    settings.setValue("dataExfiltrationKB", ui->spinDataExfiltration->value());
    settings.endGroup();
    
    // Save alert type settings
    settings.beginGroup("EnabledAlerts");
    settings.setValue("AlertType_0", ui->chkNewApp->isChecked());
    settings.setValue("AlertType_1", ui->chkHighBandwidth->isChecked());
    settings.setValue("AlertType_2", ui->chkSuspiciousConn->isChecked());
    settings.setValue("AlertType_3", ui->chkPortScan->isChecked());
    settings.setValue("AlertType_4", ui->chkDataExfiltration->isChecked());
    settings.setValue("AlertType_5", ui->chkProtocolAnomaly->isChecked());
    settings.setValue("AlertType_6", ui->chkConnectionSpike->isChecked());
    settings.setValue("AlertType_7", ui->chkRuleViolation->isChecked());
    settings.setValue("AlertType_8", ui->chkCustomAlert->isChecked());
    settings.endGroup();
    
    settings.sync();
}

void AlertSettingsDialog::setupUi()
{
    // Set up tooltips
    ui->spinBandwidthThreshold->setToolTip(tr("Alert when network bandwidth exceeds this threshold (KB/s)"));
    ui->spinConnectionWindow->setToolTip(tr("Time window in seconds to monitor for connection spikes"));
    ui->spinConnectionThreshold->setToolTip(tr("Alert when number of connections exceeds this threshold within the time window"));
    ui->spinDataExfiltration->setToolTip(tr("Alert when data upload exceeds this threshold (KB)"));
    
    // Set up tab icons
    ui->tabWidget->setTabIcon(0, QIcon(":/resources/icons/png/sliders.png"));
    ui->tabWidget->setTabIcon(1, QIcon(":/resources/icons/png/alert.png"));
    
    // Set up button box
    ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)->setIcon(QIcon(":/resources/icons/png/refresh.png"));
    ui->buttonBox->button(QDialogButtonBox::Ok)->setIcon(QIcon(":/resources/icons/png/check.png"));
    ui->buttonBox->button(QDialogButtonBox::Cancel)->setIcon(QIcon(":/resources/icons/png/close.png"));
}

void AlertSettingsDialog::setupConnections()
{
    // Connect Restore Defaults button
    connect(ui->buttonBox->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked,
            this, &AlertSettingsDialog::restoreDefaults);
    
    // Connect OK and Cancel buttons
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, &AlertSettingsDialog::on_buttonBox_accepted);
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &AlertSettingsDialog::on_buttonBox_rejected);
}

void AlertSettingsDialog::setAlertManager(AlertManager *alertManager)
{
    // Store the alert manager reference if needed for future use
    // For now, this method is just a placeholder as the dialog doesn't need to store the manager
    Q_UNUSED(alertManager);
}
