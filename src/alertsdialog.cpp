#include "alertsdialog.h"
#include "ui_alertsdialog.h"
#include "alertmanager.h"
#include "networkmonitor.h"
#include "alertsettingsdialog.h"
#include <QMenu>
#include <QMessageBox>
#include <QDateTime>
#include <QClipboard>
#include <QApplication>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QSettings>
#include <QFormLayout>
#include <QStatusBar>

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
    ui(new Ui::AlertsDialog()),
    m_alertManager(nullptr)
{
    // Initialize UI first
    ui->setupUi(this);
    
    // Set window flags to make it a proper window
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    
    // Initialize models after UI
    m_activeAlertsModel = new QStandardItemModel(this);
    m_alertHistoryModel = new QStandardItemModel(this);
    
    // Set up UI components
    setupUi();
    
    // Set up models after UI is ready
    setupModels();
    
    // Connect signals and slots
    setupConnections();
    
    // Restore window state
    QSettings settings("NetWire", "NetWire");
    restoreGeometry(settings.value("AlertsDialog/geometry").toByteArray());
}

AlertsDialog::~AlertsDialog()
{
    // Save window state
    QSettings settings("NetWire", "NetWire");
    settings.setValue("AlertsDialog/geometry", saveGeometry());
    
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
    
    QDialog::closeEvent(event);
}

void AlertsDialog::setupModels()
{
    // Get the column count from the enum
    const int columnCount = AlertsDialog::ColCount;
    
    // Active alerts model
    m_activeAlertsModel->setColumnCount(columnCount);
    m_activeAlertsModel->setHorizontalHeaderLabels({
        tr("ID"), tr("Severity"), tr("Type"), tr("Title"), 
        tr("Source"), tr("Destination"), tr("Time"), tr("Acknowledged"), ""
    });
    
    // Alert history model
    m_alertHistoryModel->setColumnCount(columnCount);
    m_alertHistoryModel->setHorizontalHeaderLabels({
        tr("ID"), tr("Severity"), tr("Type"), tr("Title"), 
        tr("Source"), tr("Destination"), tr("Time"), tr("Acknowledged"), ""
    });
    
    // Set up table views
    if (ui->tblAlerts) {
        ui->tblAlerts->setModel(m_activeAlertsModel);
        
        // Hide the alert data column
        ui->tblAlerts->setColumnHidden(AlertsDialog::ColAlertData, true);
        
        // Set up selection behavior
        ui->tblAlerts->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->tblAlerts->setSelectionMode(QAbstractItemView::ExtendedSelection);
        
        // Enable sorting
        ui->tblAlerts->setSortingEnabled(true);
        
        // Set column widths
        ui->tblAlerts->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        ui->tblAlerts->horizontalHeader()->setSectionResizeMode(AlertsDialog::ColTitle, QHeaderView::Stretch);
        
        // Set up context menu
        ui->tblAlerts->setContextMenuPolicy(Qt::CustomContextMenu);
    }
    
    if (ui->tblHistory) {
        ui->tblHistory->setModel(m_alertHistoryModel);
        
        // Hide the alert data column
        ui->tblHistory->setColumnHidden(AlertsDialog::ColAlertData, true);
        
        // Set up selection behavior
        ui->tblHistory->setSelectionBehavior(QAbstractItemView::SelectRows);
        ui->tblHistory->setSelectionMode(QAbstractItemView::ExtendedSelection);
        
        // Enable sorting
        ui->tblHistory->setSortingEnabled(true);
        
        // Set column widths
        ui->tblHistory->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        ui->tblHistory->horizontalHeader()->setSectionResizeMode(AlertsDialog::ColTitle, QHeaderView::Stretch);
        
        // Set up context menu
        ui->tblHistory->setContextMenuPolicy(Qt::CustomContextMenu);
    }
}

void AlertsDialog::setupUi()
{
    // Set up icons
    ui->btnAcknowledge->setIcon(QIcon(":/resources/icons/png/check.png"));
    ui->btnClear->setIcon(QIcon(":/resources/icons/png/delete.png"));
    ui->btnClearAll->setIcon(QIcon(":/resources/icons/png/delete_all.png"));
    
    // Set up filter placeholders
    ui->txtFilter->setPlaceholderText(tr("Filter alerts..."));
    ui->txtHistoryFilter->setPlaceholderText(tr("Filter history..."));
    
    // Set up tooltips
    ui->btnAcknowledge->setToolTip(tr("Acknowledge selected alerts"));
    ui->btnClear->setToolTip(tr("Clear selected alerts"));
    ui->btnClearAll->setToolTip(tr("Clear all alerts"));
    
    // Set up tab icons
    ui->tabWidget->setTabIcon(0, QIcon(":/resources/icons/png/alert.png"));
    ui->tabWidget->setTabIcon(1, QIcon(":/resources/icons/png/history.png"));
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
    
    // Create a unique ID using a hash of the alert's title and timestamp
    QString uniqueId = QString("%1-%2").arg(alert.title).arg(alert.timestamp.toMSecsSinceEpoch());
    uint id = qHash(uniqueId);
    
    // ID
    QStandardItem *idItem = new QStandardItem(QString::number(id));
    idItem->setData(id, Qt::UserRole);
    
    // Severity with icon
    QStandardItem *severityItem = new QStandardItem();
    severityItem->setData(alert.severity, Qt::UserRole);
    
    // Set icon based on severity
    switch (alert.severity) {
    case AlertManager::Info:
        severityItem->setIcon(QIcon(":/resources/icons/png/info.png"));
        break;
    case AlertManager::Low:
        severityItem->setIcon(QIcon(":/resources/icons/png/info_blue.png"));
        break;
    case AlertManager::Medium:
        severityItem->setIcon(QIcon(":/resources/icons/png/warning.png"));
        break;
    case AlertManager::High:
        severityItem->setIcon(QIcon(":/resources/icons/png/error.png"));
        break;
    case AlertManager::Critical:
        severityItem->setIcon(QIcon(":/resources/icons/png/critical.png"));
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
    
    // Show the message in a message box since we don't have a status bar
    QMessageBox::information(this, tr("Alert Threshold Reached"), message);
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
    QAction *detailsAction = menu.addAction(QIcon(":/resources/icons/png/info.png"), tr("View Details"));
    menu.addSeparator();
    
    // Active alerts specific actions
    if (tableView == ui->tblAlerts) {
        QAction *ackAction = menu.addAction(QIcon(":/resources/icons/png/check.png"), tr("Acknowledge"));
        QAction *clearAction = menu.addAction(QIcon(":/resources/icons/png/delete.png"), tr("Clear"));
        
        // Connect actions using new-style connect syntax
        connect(detailsAction, &QAction::triggered, this, [this]() { this->showAlertDetails(); });
        connect(ackAction, &QAction::triggered, this, [this]() { this->acknowledgeSelected(); });
        connect(clearAction, &QAction::triggered, this, [this]() { this->clearSelected(); });
    } else {
        // History specific actions
        QAction *copyAction = menu.addAction(QIcon(":/resources/icons/png/copy.png"), tr("Copy to Clipboard"));
        
        // Connect actions using new-style connect syntax
        connect(detailsAction, &QAction::triggered, this, [this]() { this->showAlertDetails(); });
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
                if (auto *sb = findChild<QStatusBar*>()) {
                    sb->showMessage(tr("Alert details copied to clipboard"), 3000);
                }
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
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle(tr("Alert Details"));
    dialog->setWindowIcon(QIcon(":/resources/icons/png/info.png"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->resize(500, 400);
    
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    
    // Title with alert type and severity
    QString titleText = QString("%1 - %2").arg(
        alertSeverityToString(alert.severity),
        alert.title
    );
    
    QLabel *titleLabel = new QLabel(titleText, dialog);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    titleLabel->setWordWrap(true);
    
    // Set text color based on severity
    QPalette palette = titleLabel->palette();
    switch (alert.severity) {
        case AlertManager::Critical:
            palette.setColor(QPalette::WindowText, Qt::red);
            break;
        case AlertManager::High:
            palette.setColor(QPalette::WindowText, QColor(255, 165, 0)); // Orange
            break;
        case AlertManager::Medium:
            palette.setColor(QPalette::WindowText, QColor(255, 215, 0)); // Gold
            break;
        case AlertManager::Low:
            palette.setColor(QPalette::WindowText, Qt::blue);
            break;
        case AlertManager::Info:
        default:
            palette.setColor(QPalette::WindowText, Qt::darkGreen);
            break;
    }
    titleLabel->setPalette(palette);
    
    // Timestamp
    QLabel *timeLabel = new QLabel(
        tr("Time: %1").arg(alert.timestamp.toString("yyyy-MM-dd hh:mm:ss")),
        dialog
    );
    
    // Main form layout for alert details
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight);
    formLayout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    
    // Add alert details to form
    formLayout->addRow(tr("Type:"), new QLabel(alertTypeToString(alert.type), dialog));
    formLayout->addRow(tr("Source:"), new QLabel(alert.source, dialog));
    formLayout->addRow(tr("Destination:"), new QLabel(alert.destination, dialog));
    
    if (alert.bytesTransferred > 0) {
        formLayout->addRow(tr("Data transferred:"), new QLabel(formatBytes(alert.bytesTransferred), dialog));
    }
    
    // Description
    QTextEdit *descriptionEdit = new QTextEdit(alert.description, dialog);
    descriptionEdit->setReadOnly(true);
    descriptionEdit->setFrameStyle(QFrame::NoFrame);
    descriptionEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::MinimumExpanding);
    
    // Additional info if available
    QGroupBox *infoGroup = nullptr;
    if (!alert.additionalInfo.isEmpty()) {
        infoGroup = new QGroupBox(tr("Additional Information"), dialog);
        QVBoxLayout *infoLayout = new QVBoxLayout(infoGroup);
        QTextEdit *infoEdit = new QTextEdit(alert.additionalInfo, infoGroup);
        infoEdit->setReadOnly(true);
        infoEdit->setFrameStyle(QFrame::NoFrame);
        infoLayout->addWidget(infoEdit);
    }
    
    // Buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    connect(buttonBox, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    
    // Add widgets to main layout
    layout->addWidget(titleLabel);
    layout->addWidget(timeLabel);
    layout->addSpacing(10);
    layout->addLayout(formLayout);
    layout->addSpacing(10);
    layout->addWidget(new QLabel(tr("Description:"), dialog));
    layout->addWidget(descriptionEdit, 1);
    
    if (infoGroup) {
        layout->addSpacing(10);
        layout->addWidget(infoGroup, 1);
    }
    
    layout->addSpacing(10);
    layout->addWidget(buttonBox);
    
    // Show the dialog
    dialog->setLayout(layout);
    dialog->show();
    dialog->exec();
}

QString AlertsDialog::alertTypeToString(AlertManager::AlertType type)
{
    switch (type) {
    case AlertManager::NewAppDetected: return QObject::tr("New Application Detected");
    case AlertManager::HighBandwidthUsage: return QObject::tr("High Bandwidth Usage");
    case AlertManager::SuspiciousConnection: return QObject::tr("Suspicious Connection");
    case AlertManager::PortScanDetected: return QObject::tr("Port Scan Detected");
    case AlertManager::DataExfiltration: return QObject::tr("Data Exfiltration");
    case AlertManager::ProtocolAnomaly: return QObject::tr("Protocol Anomaly");
    case AlertManager::ConnectionSpike: return QObject::tr("Connection Spike");
    case AlertManager::RuleViolation: return QObject::tr("Rule Violation");
    case AlertManager::CustomAlert: return QObject::tr("Custom Alert");
    default: return QObject::tr("Unknown");
    }
}

QString AlertsDialog::alertSeverityToString(AlertManager::Severity severity)
{
    switch (severity) {
    case AlertManager::Info: return QObject::tr("Info");
    case AlertManager::Low: return QObject::tr("Low");
    case AlertManager::Medium: return QObject::tr("Medium");
    case AlertManager::High: return QObject::tr("High");
    case AlertManager::Critical: return QObject::tr("Critical");
    default: return QObject::tr("Unknown");
    }
}

QString AlertsDialog::formatBytes(qint64 bytes)
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
