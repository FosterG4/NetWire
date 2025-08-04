#include "alertsdialog.h"
#include "ui_alertsdialog.h"
#include "alertmanager.h"
#include "networkmonitor.h"
#include <QMenu>
#include <QMessageBox>
#include <QDateTime>
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QSettings>

class AlertFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
    
public:
    explicit AlertFilterProxyModel(QObject *parent = nullptr)
        : QSortFilterProxyModel(parent), m_filterText()
    {
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setFilterKeyColumn(-1); // Filter on all columns
    }
    
    void setFilterText(const QString &text) {
        m_filterText = text;
        invalidateFilter();
    }
    
protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override {
        if (m_filterText.isEmpty()) {
            return true;
        }
        
        QModelIndex index = sourceModel()->index(sourceRow, 0, sourceParent);
        for (int col = 0; col < sourceModel()->columnCount(sourceParent); ++col) {
            QString text = sourceModel()->data(sourceModel()->index(sourceRow, col, sourceParent), 
                                             Qt::DisplayRole).toString();
            if (text.contains(m_filterText, Qt::CaseInsensitive)) {
                return true;
            }
        }
        return false;
    }
    
private:
    QString m_filterText;
};

AlertsDialog::AlertsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AlertsDialog),
    m_alertManager(nullptr),
    m_activeAlertsModel(new QStandardItemModel(this)),
    m_alertHistoryModel(new QStandardItemModel(this))
{
    ui->setupUi(this);
    
    // Set window flags to make it a proper window
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    // Set up models
    setupModels();
    
    // Set up UI
    setupUi();
    
    // Connect signals and slots
    setupConnections();
    
    // Restore window state
    QSettings settings("NetWire", "NetWire");
    restoreGeometry(settings.value("AlertsDialog/geometry").toByteArray());
    ui->splitter->restoreState(settings.value("AlertsDialog/splitterState").toByteArray());
}

AlertsDialog::~AlertsDialog()
{
    // Save window state
    QSettings settings("NetWire", "NetWire");
    settings.setValue("AlertsDialog/geometry", saveGeometry());
    settings.setValue("AlertsDialog/splitterState", ui->splitter->saveState());
    
    delete ui;
}

void AlertsDialog::setAlertManager(AlertManager *manager)
{
    if (m_alertManager == manager) {
        return;
    }
    
    // Disconnect old manager if any
    if (m_alertManager) {
        disconnect(m_alertManager, &AlertManager::newAlert,
                  this, &AlertsDialog::onNewAlert);
        disconnect(m_alertManager, &AlertManager::alertAcknowledged,
                  this, &AlertsDialog::onAlertAcknowledged);
        disconnect(m_alertManager, &AlertManager::alertCleared,
                  this, &AlertsDialog::onAlertCleared);
        disconnect(m_alertManager, &AlertManager::alertThresholdReached,
                  this, &AlertsDialog::onThresholdReached);
    }
    
    m_alertManager = manager;
    
    if (m_alertManager) {
        // Connect to the new manager
        connect(m_alertManager, &AlertManager::newAlert,
                this, &AlertsDialog::onNewAlert);
        connect(m_alertManager, &AlertManager::alertAcknowledged,
                this, &AlertsDialog::onAlertAcknowledged);
        connect(m_alertManager, &AlertManager::alertCleared,
                this, &AlertsDialog::onAlertCleared);
        connect(m_alertManager, &AlertManager::alertThresholdReached,
                this, &AlertsDialog::onThresholdReached);
        
        // Update the UI with current alerts
        updateAlerts();
    }
}

void AlertsDialog::updateAlerts()
{
    if (!m_alertManager) {
        return;
    }
    
    updateActiveAlerts();
    updateAlertHistory();
}

void AlertsDialog::showEvent(QShowEvent *event)
{
    Q_UNUSED(event);
    updateAlerts();
}

void AlertsDialog::closeEvent(QCloseEvent *event)
{
    // Save window state
    QSettings settings("NetWire", "NetWire");
    settings.setValue("AlertsDialog/geometry", saveGeometry());
    settings.setValue("AlertsDialog/splitterState", ui->splitter->saveState());
    
    QDialog::closeEvent(event);
}

void AlertsDialog::setupModels()
{
    // Active alerts model
    m_activeAlertsModel->setColumnCount(ColCount);
    m_activeAlertsModel->setHorizontalHeaderLabels({
        tr("ID"), tr("Severity"), tr("Type"), tr("Title"), 
        tr("Source"), tr("Destination"), tr("Time"), tr("Acknowledged"), ""
    });
    
    // Alert history model
    m_alertHistoryModel->setColumnCount(ColCount);
    m_alertHistoryModel->setHorizontalHeaderLabels({
        tr("ID"), tr("Severity"), tr("Type"), tr("Title"), 
        tr("Source"), tr("Destination"), tr("Time"), tr("Acknowledged"), ""
    });
    
    // Set up table views
    ui->tblAlerts->setModel(m_activeAlertsModel);
    ui->tblHistory->setModel(m_alertHistoryModel);
    
    // Hide the alert data column
    ui->tblAlerts->setColumnHidden(ColAlertData, true);
    ui->tblHistory->setColumnHidden(ColAlertData, true);
    
    // Set up selection behavior
    ui->tblAlerts->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblAlerts->setSelectionMode(QAbstractItemView::ExtendedSelection);
    ui->tblHistory->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblHistory->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    // Enable sorting
    ui->tblAlerts->setSortingEnabled(true);
    ui->tblHistory->setSortingEnabled(true);
    
    // Set column widths
    ui->tblAlerts->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tblAlerts->horizontalHeader()->setSectionResizeMode(ColTitle, QHeaderView::Stretch);
    ui->tblHistory->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->tblHistory->horizontalHeader()->setSectionResizeMode(ColTitle, QHeaderView::Stretch);
    
    // Set up context menu
    ui->tblAlerts->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tblHistory->setContextMenuPolicy(Qt::CustomContextMenu);
}

void AlertsDialog::setupUi()
{
    // Set up icons
    ui->btnAcknowledge->setIcon(QIcon(":/icons/check.png"));
    ui->btnClear->setIcon(QIcon(":/icons/delete.png"));
    ui->btnClearAll->setIcon(QIcon(":/icons/delete_all.png"));
    
    // Set up filter placeholders
    ui->txtFilter->setPlaceholderText(tr("Filter alerts..."));
    ui->txtHistoryFilter->setPlaceholderText(tr("Filter history..."));
    
    // Set up tooltips
    ui->btnAcknowledge->setToolTip(tr("Acknowledge selected alerts"));
    ui->btnClear->setToolTip(tr("Clear selected alerts"));
    ui->btnClearAll->setToolTip(tr("Clear all alerts"));
    
    // Set up tab icons
    ui->tabWidget->setTabIcon(0, QIcon(":/icons/alert.png"));
    ui->tabWidget->setTabIcon(1, QIcon(":/icons/history.png"));
}

void AlertsDialog::setupConnections()
{
    // Button connections
    connect(ui->btnAcknowledge, &QPushButton::clicked, this, &AlertsDialog::on_btnAcknowledge_clicked);
    connect(ui->btnClear, &QPushButton::clicked, this, &AlertsDialog::on_btnClear_clicked);
    connect(ui->btnClearAll, &QPushButton::clicked, this, &AlertsDialog::on_btnClearAll_clicked);
    
    // Filter connections
    connect(ui->txtFilter, &QLineEdit::textChanged, this, &AlertsDialog::on_txtFilter_textChanged);
    connect(ui->txtHistoryFilter, &QLineEdit::textChanged, this, [this](const QString &text) {
        static_cast<AlertFilterProxyModel*>(ui->tblHistory->model())->setFilterText(text);
    });
    
    // Table connections
    connect(ui->tblAlerts, &QTableView::doubleClicked, this, &AlertsDialog::on_tblAlerts_doubleClicked);
    connect(ui->tblHistory, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        Q_UNUSED(index);
        showAlertDetails();
    });
    
    // Context menu connections
    connect(ui->tblAlerts, &QTableView::customContextMenuRequested, 
            this, &AlertsDialog::showContextMenu);
    connect(ui->tblHistory, &QTableView::customContextMenuRequested, 
            this, &AlertsDialog::showContextMenu);
    
    // Tab change
    connect(ui->tabWidget, &QTabWidget::currentChanged, 
            this, &AlertsDialog::on_tabWidget_currentChanged);
}

void AlertsDialog::updateActiveAlerts()
{
    if (!m_alertManager) {
        return;
    }
    
    m_activeAlertsModel->removeRows(0, m_activeAlertsModel->rowCount());
    m_alertMap.clear();
    
    QList<AlertManager::Alert> alerts = m_alertManager->activeAlerts();
    for (const auto &alert : alerts) {
        addAlertToModel(alert, m_activeAlertsModel);
    }
    
    // Update window title with alert count
    setWindowTitle(tr("Network Alerts (%1 active)").arg(alerts.size()));
}

void AlertsDialog::updateAlertHistory()
{
    if (!m_alertManager) {
        return;
    }
    
    m_alertHistoryModel->removeRows(0, m_alertHistoryModel->rowCount());
    
    QList<AlertManager::Alert> history = m_alertManager->alertHistory();
    for (const auto &alert : history) {
        addAlertToModel(alert, m_alertHistoryModel);
    }
}

void AlertsDialog::addAlertToModel(const AlertManager::Alert &alert, QStandardItemModel *model)
{
    if (!model) return;
    
    QList<QStandardItem*> row = createAlertRow(alert);
    model->appendRow(row);
    
    // Store the alert in the map
    int id = row[ColId]->data(Qt::DisplayRole).toInt();
    m_alertMap[id] = alert;
}

QList<QStandardItem*> AlertsDialog::createAlertRow(const AlertManager::Alert &alert) const
{
    QList<QStandardItem*> row;
    
    // ID
    QStandardItem *idItem = new QStandardItem(QString::number(alert.id));
    idItem->setData(alert.id, Qt::UserRole);
    
    // Severity with icon
    QStandardItem *severityItem = new QStandardItem();
    severityItem->setData(alert.severity, Qt::UserRole);
    
    // Set icon based on severity
    switch (alert.severity) {
    case AlertManager::Info:
        severityItem->setIcon(QIcon(":/icons/info.png"));
        break;
    case AlertManager::Low:
        severityItem->setIcon(QIcon(":/icons/info_blue.png"));
        break;
    case AlertManager::Medium:
        severityItem->setIcon(QIcon(":/icons/warning.png"));
        break;
    case AlertManager::High:
        severityItem->setIcon(QIcon(":/icons/error.png"));
        break;
    case AlertManager::Critical:
        severityItem->setIcon(QIcon(":/icons/critical.png"));
        break;
    }
    
    // Type
    QStandardItem *typeItem = new QStandardItem(alertTypeToString(alert.type));
    
    // Title
    QStandardItem *titleItem = new QStandardItem(alert.title);
    
    // Source and destination
    QStandardItem *sourceItem = new QStandardItem(alert.source);
    QStandardItem *destItem = new QStandardItem(alert.destination);
    
    // Timestamp
    QStandardItem *timeItem = new QStandardItem(alert.timestamp.toString("yyyy-MM-dd hh:mm:ss"));
    
    // Acknowledged
    QStandardItem *ackItem = new QStandardItem(alert.acknowledged ? tr("Yes") : tr("No"));
    
    // Alert data (hidden)
    QStandardItem *dataItem = new QStandardItem();
    dataItem->setData(QVariant::fromValue(alert), Qt::UserRole);
    
    // Add all items to the row
    row << idItem << severityItem << typeItem << titleItem 
        << sourceItem << destItem << timeItem << ackItem << dataItem;
    
    return row;
}

void AlertsDialog::onNewAlert(const AlertManager::Alert &alert)
{
    // Add to active alerts
    addAlertToModel(alert, m_activeAlertsModel);
    
    // Add to history
    addAlertToModel(alert, m_alertHistoryModel);
    
    // Update window title with new count
    setWindowTitle(tr("Network Alerts (%1 active)").arg(m_activeAlertsModel->rowCount()));
    
    // Show system notification if window is not active
    if (!isActiveWindow()) {
        // Emit signal for main window to show system tray notification
        emit alertReceived(alert);
    }
}

void AlertsDialog::onAlertAcknowledged(int alertId)
{
    // Find and update the alert in active alerts
    for (int i = 0; i < m_activeAlertsModel->rowCount(); ++i) {
        QStandardItem *idItem = m_activeAlertsModel->item(i, ColId);
        if (idItem && idItem->data(Qt::DisplayRole).toInt() == alertId) {
            // Update acknowledged status
            QStandardItem *ackItem = m_activeAlertsModel->item(i, ColAcknowledged);
            if (ackItem) {
                ackItem->setText(tr("Yes"));
            }
            
            // Update the alert in the map
            if (m_alertMap.contains(alertId)) {
                AlertManager::Alert alert = m_alertMap[alertId];
                alert.acknowledged = true;
                m_alertMap[alertId] = alert;
            }
            
            break;
        }
    }
}

void AlertsDialog::onAlertCleared(int alertId)
{
    // Remove from active alerts
    for (int i = 0; i < m_activeAlertsModel->rowCount(); ++i) {
        QStandardItem *idItem = m_activeAlertsModel->item(i, ColId);
        if (idItem && idItem->data(Qt::DisplayRole).toInt() == alertId) {
            m_activeAlertsModel->removeRow(i);
            m_alertMap.remove(alertId);
            break;
        }
    }
    
    // Update window title with new count
    setWindowTitle(tr("Network Alerts (%1 active)").arg(m_activeAlertsModel->rowCount()));
}

void AlertsDialog::onThresholdReached(AlertManager::AlertType type, const QString &message)
{
    Q_UNUSED(type);
    
    // Show a status bar message or tooltip
    statusBar()->showMessage(message, 5000);
}

void AlertsDialog::on_tabWidget_currentChanged(int index)
{
    Q_UNUSED(index);
    // Update the view when switching tabs
    if (ui->tabWidget->currentIndex() == 0) {
        updateActiveAlerts();
    } else {
        updateAlertHistory();
    }
}

void AlertsDialog::on_btnAcknowledge_clicked()
{
    QItemSelectionModel *selection = ui->tblAlerts->selectionModel();
    if (!selection->hasSelection() || !m_alertManager) {
        return;
    }
    
    QModelIndexList selected = selection->selectedRows(ColId);
    for (const QModelIndex &index : selected) {
        int alertId = index.data(Qt::DisplayRole).toInt();
        m_alertManager->acknowledgeAlert(alertId);
    }
}

void AlertsDialog::on_btnClear_clicked()
{
    QItemSelectionModel *selection = ui->tblAlerts->selectionModel();
    if (!selection->hasSelection() || !m_alertManager) {
        return;
    }
    
    QModelIndexList selected = selection->selectedRows(ColId);
    for (const QModelIndex &index : selected) {
        int alertId = index.data(Qt::DisplayRole).toInt();
        m_alertManager->clearAlert(alertId);
    }
}

void AlertsDialog::on_btnClearAll_clicked()
{
    if (!m_alertManager) {
        return;
    }
    
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("Clear All Alerts"),
                                tr("Are you sure you want to clear all active alerts?"),
                                QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        m_alertManager->clearAllAlerts();
        m_activeAlertsModel->removeRows(0, m_activeAlertsModel->rowCount());
        m_alertMap.clear();
        setWindowTitle(tr("Network Alerts (0 active)"));
    }
}

void AlertsDialog::on_btnSettings_clicked()
{
    // Show alert settings dialog
    AlertSettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // Apply settings
        AlertManager::ThresholdConfig config = dialog.thresholdConfig();
        m_alertManager->setThresholdConfig(config);
        
        // Update enabled alert types
        for (int i = 0; i <= AlertManager::CustomAlert; ++i) {
            AlertManager::AlertType type = static_cast<AlertManager::AlertType>(i);
            bool enabled = dialog.isAlertTypeEnabled(type);
            m_alertManager->setAlertTypeEnabled(type, enabled);
        }
    }
}

void AlertsDialog::on_tblAlerts_doubleClicked(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }
    
    showAlertDetails();
}

void AlertsDialog::on_txtFilter_textChanged(const QString &text)
{
    // Filter active alerts
    QSortFilterProxyModel *proxy = qobject_cast<QSortFilterProxyModel*>(ui->tblAlerts->model());
    if (!proxy) {
        proxy = new QSortFilterProxyModel(this);
        proxy->setSourceModel(m_activeAlertsModel);
        ui->tblAlerts->setModel(proxy);
    }
    
    proxy->setFilterFixedString(text);
    proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    proxy->setFilterKeyColumn(-1); // Filter on all columns
}

void AlertsDialog::showContextMenu(const QPoint &pos)
{
    QTableView *tableView = qobject_cast<QTableView*>(sender());
    if (!tableView) {
        return;
    }
    
    QModelIndex index = tableView->indexAt(pos);
    if (!index.isValid()) {
        return;
    }
    
    QMenu menu(this);
    
    // Common actions
    QAction *detailsAction = menu.addAction(QIcon(":/icons/info.png"), tr("View Details"));
    menu.addSeparator();
    
    // Active alerts specific actions
    if (tableView == ui->tblAlerts) {
        QAction *ackAction = menu.addAction(QIcon(":/icons/check.png"), tr("Acknowledge"));
        QAction *clearAction = menu.addAction(QIcon(":/icons/delete.png"), tr("Clear"));
        
        // Connect actions
        connect(detailsAction, &QAction::triggered, this, &AlertsDialog::showAlertDetails);
        connect(ackAction, &QAction::triggered, this, &AlertsDialog::acknowledgeSelected);
        connect(clearAction, &QAction::triggered, this, &AlertsDialog::clearSelected);
    } else {
        // History specific actions
        QAction *copyAction = menu.addAction(QIcon(":/icons/copy.png"), tr("Copy to Clipboard"));
        
        // Connect actions
        connect(detailsAction, &QAction::triggered, this, &AlertsDialog::showAlertDetails);
        connect(copyAction, &QAction::triggered, this, [this]() {
            QItemSelectionModel *selection = ui->tblHistory->selectionModel();
            if (!selection->hasSelection()) {
                return;
            }
            
            QModelIndex index = selection->selectedRows(ColId).first();
            int alertId = index.data(Qt::DisplayRole).toInt();
            
            if (m_alertMap.contains(alertId)) {
                const AlertManager::Alert &alert = m_alertMap[alertId];
                QString text = QString("%1\n%2\nSeverity: %3\nTime: %4\nSource: %5\nDestination: %6")
                                  .arg(alert.title)
                                  .arg(alert.description)
                                  .arg(alertSeverityToString(alert.severity))
                                  .arg(alert.timestamp.toString("yyyy-MM-dd hh:mm:ss"))
                                  .arg(alert.source)
                                  .arg(alert.destination);
                
                QApplication::clipboard()->setText(text);
                statusBar()->showMessage(tr("Alert details copied to clipboard"), 3000);
            }
        });
    }
    
    // Show the context menu
    menu.exec(tableView->viewport()->mapToGlobal(pos));
}

void AlertsDialog::acknowledgeSelected()
{
    on_btnAcknowledge_clicked();
}

void AlertsDialog::clearSelected()
{
    on_btnClear_clicked();
}

void AlertsDialog::showAlertDetails()
{
    QTableView *tableView = qobject_cast<QTableView*>(sender());
    if (!tableView) {
        // Called from double-click or menu
        tableView = ui->tblAlerts->hasFocus() ? ui->tblAlerts : ui->tblHistory;
    }
    
    QModelIndexList selected = tableView->selectionModel()->selectedRows(ColId);
    if (selected.isEmpty()) {
        return;
    }
    
    int alertId = selected.first().data(Qt::DisplayRole).toInt();
    
    if (m_alertMap.contains(alertId)) {
        showAlertDetails(m_alertMap[alertId]);
    }
}

void AlertsDialog::showAlertDetails(const AlertManager::Alert &alert)
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Alert Details"));
    dialog.setWindowIcon(QIcon(":/icons/info.png"));
    dialog.resize(500, 400);
    
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    
    // Title
    QLabel *titleLabel = new QLabel(alert.title);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    
    // Description
    QLabel *descLabel = new QLabel(alert.description);
    descLabel->setWordWrap(true);
    
    // Details
    QGroupBox *detailsGroup = new QGroupBox(tr("Details"));
    QFormLayout *detailsLayout = new QFormLayout(detailsGroup);
    
    detailsLayout->addRow(tr("Type:"), new QLabel(alertTypeToString(alert.type)));
    detailsLayout->addRow(tr("Severity:"), new QLabel(alertSeverityToString(alert.severity)));
    detailsLayout->addRow(tr("Time:"), new QLabel(alert.timestamp.toString("yyyy-MM-dd hh:mm:ss")));
    
    if (!alert.source.isEmpty()) {
        detailsLayout->addRow(tr("Source:"), new QLabel(alert.source));
    }
    
    if (!alert.destination.isEmpty()) {
        detailsLayout->addRow(tr("Destination:"), new QLabel(alert.destination));
    }
    
    if (alert.bytesTransferred > 0) {
        detailsLayout->addRow(tr("Data:"), new QLabel(formatBytes(alert.bytesTransferred)));
    }
    
    detailsGroup->setLayout(detailsLayout);
    
    // Additional info
    if (!alert.additionalInfo.isEmpty()) {
        QGroupBox *infoGroup = new QGroupBox(tr("Additional Information"));
        QVBoxLayout *infoLayout = new QVBoxLayout(infoGroup);
        
        QTextEdit *infoEdit = new QTextEdit();
        infoEdit->setPlainText(alert.additionalInfo);
        infoEdit->setReadOnly(true);
        
        infoLayout->addWidget(infoEdit);
        infoGroup->setLayout(infoLayout);
    }
    
    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    
    // Add widgets to layout
    layout->addWidget(titleLabel);
    layout->addWidget(descLabel);
    layout->addSpacing(10);
    layout->addWidget(detailsGroup);
    
    if (!alert.additionalInfo.isEmpty()) {
        layout->addSpacing(10);
        layout->addWidget(infoGroup);
    }
    
    layout->addStretch();
    layout->addWidget(buttonBox);
    
    dialog.setLayout(layout);
    dialog.exec();
}

QString AlertsDialog::alertTypeToString(AlertManager::AlertType type) const
{
    switch (type) {
    case AlertManager::NewAppDetected: return tr("New Application Detected");
    case AlertManager::HighBandwidthUsage: return tr("High Bandwidth Usage");
    case AlertManager::SuspiciousConnection: return tr("Suspicious Connection");
    case AlertManager::PortScanDetected: return tr("Port Scan Detected");
    case AlertManager::DataExfiltration: return tr("Data Exfiltration");
    case AlertManager::ProtocolAnomaly: return tr("Protocol Anomaly");
    case AlertManager::ConnectionSpike: return tr("Connection Spike");
    case AlertManager::RuleViolation: return tr("Rule Violation");
    case AlertManager::CustomAlert: return tr("Custom Alert");
    default: return tr("Unknown");
    }
}

QString AlertsDialog::alertSeverityToString(AlertManager::Severity severity) const
{
    switch (severity) {
    case AlertManager::Info: return tr("Info");
    case AlertManager::Low: return tr("Low");
    case AlertManager::Medium: return tr("Medium");
    case AlertManager::High: return tr("High");
    case AlertManager::Critical: return tr("Critical");
    default: return tr("Unknown");
    }
}

QString AlertsDialog::formatBytes(qint64 bytes) const
{
    const char *suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double size = bytes;
    
    while (size >= 1024 && i < 4) {
        size /= 1024;
        i++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(suffixes[i]);
}

#include "alertsdialog.moc"
