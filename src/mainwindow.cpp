#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "alertmanager.h"
#include "alertsettingsdialog.h"
#include <QMessageBox>
#include <QMenu>
#include <QFileDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QHeaderView>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QDebug>
#include <QStyleFactory>
#include <QApplication>
#include <QPalette>
#include <QToolBar>
#include <QMenuBar>
#include <QStatusBar>
#include <QInputDialog>
#include <QCloseEvent>

// ApplicationTableModel implementation
ApplicationTableModel::ApplicationTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
    m_lastUpdate = QDateTime::currentDateTime();
}

int ApplicationTableModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return m_apps.size();
}

int ApplicationTableModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 4; // Icon, Name, Download, Upload
}

QVariant ApplicationTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_apps.size())
        return QVariant();

    const auto &app = m_apps.at(index.row());
    
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0: return app.name;
        case 1: return app.downloadSpeed;
        case 2: return app.uploadSpeed;
        case 3: return formatBytes(app.download + app.upload);
        }
    } else if (role == Qt::DecorationRole && index.column() == 0) {
        return app.icon;
    } else if (role == Qt::TextAlignmentRole) {
        return Qt::AlignLeft + Qt::AlignVCenter;
    } else if (role == Qt::UserRole) {
        return app.name; // For sorting
    }
    
    return QVariant();
}

QVariant ApplicationTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0: return tr("Application");
        case 1: return tr("Download");
        case 2: return tr("Upload");
        case 3: return tr("Total");
        }
    }
    return QVariant();
}

void ApplicationTableModel::updateData(const QMap<QString, NetworkMonitor::NetworkStats> &stats)
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 timeDelta = m_lastUpdate.msecsTo(now);
    
    if (timeDelta < 1000) // Don't update more than once per second
        return;
    
    beginResetModel();
    
    // Store previous values for speed calculation
    QMap<QString, quint64> prevDownload = m_previousDownload;
    QMap<QString, quint64> prevUpload = m_previousUpload;
    
    m_previousDownload.clear();
    m_previousUpload.clear();
    m_apps.clear();
    
    // Update application data
    for (auto it = stats.constBegin(); it != stats.constEnd(); ++it) {
        const QString &appName = it.key();
        const NetworkMonitor::NetworkStats &stat = it.value();
        
        m_previousDownload[appName] = stat.bytesReceived;
        m_previousUpload[appName] = stat.bytesSent;
        
        double downloadSpeed = 0;
        double uploadSpeed = 0;
        
        if (prevDownload.contains(appName) && prevUpload.contains(appName)) {
            qint64 dlDiff = stat.bytesReceived - prevDownload[appName];
            qint64 ulDiff = stat.bytesSent - prevUpload[appName];
            
            // Convert to bytes per second
            downloadSpeed = (dlDiff * 1000.0) / timeDelta;
            uploadSpeed = (ulDiff * 1000.0) / timeDelta;
        }
        
        AppData appData;
        appData.name = appName;
        appData.icon = stat.processIcon.isNull() ? QIcon(":/icons/application.ico") : stat.processIcon;
        appData.download = stat.bytesReceived;
        appData.upload = stat.bytesSent;
        appData.downloadSpeed = formatSpeed(downloadSpeed);
        appData.uploadSpeed = formatSpeed(uploadSpeed);
        
        m_apps.append(appData);
    }
    
    // Sort by total data (download + upload) in descending order
    std::sort(m_apps.begin(), m_apps.end(), 
        [](const AppData &a, const AppData &b) {
            return (a.download + a.upload) > (b.download + b.upload);
        });
    
    m_lastUpdate = now;
    endResetModel();
}

// Helper function to format speed
QString formatSpeed(double bytesPerSecond)
{
    const char *units[] = {"B/s", "KB/s", "MB/s", "GB/s"};
    int unit = 0;
    double speed = bytesPerSecond;
    
    while (speed >= 1024.0 && unit < 3) {
        speed /= 1024.0;
        unit++;
    }
    
    return QString("%1 %2").arg(speed, 0, 'f', unit > 0 ? 1 : 0).arg(units[unit]);
}

// Helper function to format bytes
QString formatBytes(quint64 bytes)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', unit > 0 ? 1 : 0).arg(units[unit]);
}

// MainWindow implementation

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_networkMonitor(new NetworkMonitor(this))
    , m_currentTheme(Theme::System)
    , m_trafficChart(new QChart)
    , m_downloadSeries(new QLineSeries)
    , m_uploadSeries(new QLineSeries)
    , m_axisX(new QValueAxis)
    , m_axisY(new QValueAxis)
    , m_appModel(new ApplicationTableModel(this))
    , m_sortFilterModel(new QSortFilterProxyModel(this))
    , m_totalDownloaded(0)
    , m_totalUploaded(0)
    , m_firewallDialog(nullptr)
    , m_alertManager(new AlertManager(this))
    , m_alertsDialog(new AlertsDialog(this))
    , m_isMonitoring(false)
{
    ui->setupUi(this);
    
    // Initialize UI components
    setupUI();
    setupConnections();
    setupSystemTray();
    
    // Set initial theme
    loadTheme();
    
    // Update interface list
    updateInterfaceList();
    
    // Start with monitoring disabled
    setMonitoring(false);
    
    // Initialize firewall dialog (will be created on demand)
    m_firewallDialog = new FirewallRulesDialog(this);
}

MainWindow::~MainWindow()
{
    // Save window state and geometry
    QSettings settings("NetWire", "NetWire");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    
    delete ui;
}

void MainWindow::setupUI()
{
    // Initialize alerts dialog
    m_alertsDialog->setAlertManager(m_alertManager);
    
    // Set window properties
    setWindowTitle(tr("NetWire - Network Monitor"));
    resize(1024, 768);
    
    // Setup menu bar
    setupMenuBar();
    
    // Setup toolbar
    setupToolbar();
    
    // Setup status bar
    statusBar()->showMessage(tr("Ready"));
    
    // Setup central widget and layout
    QWidget *centralWidget = new QWidget(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);
    
    // Create tabs
    QTabWidget *tabWidget = new QTabWidget(centralWidget);
    
    // Dashboard tab
    m_dashboardWidget = new DashboardWidget(tabWidget);
    tabWidget->addTab(m_dashboardWidget, tr("Dashboard"));
    
    // Applications tab
    QWidget *appsTab = new QWidget(tabWidget);
    QVBoxLayout *appsLayout = new QVBoxLayout(appsTab);
    
    // Applications table
    m_appsTable = new QTableView(appsTab);
    m_sortFilterModel->setSourceModel(m_appModel);
    m_appsTable->setModel(m_sortFilterModel);
    m_appsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_appsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_appsTable->setSortingEnabled(true);
    m_appsTable->sortByColumn(0, Qt::AscendingOrder);
    appsLayout->addWidget(m_appsTable);
    
    // Connections tab
    QWidget *connectionsTab = new QWidget(tabWidget);
    QVBoxLayout *connectionsLayout = new QVBoxLayout(connectionsTab);
    
    // Connections table
    m_connectionsTable = new QTableWidget(0, 5, connectionsTab);
    QStringList headers = {tr("Protocol"), tr("Local Address"), tr("Remote Address"), 
                          tr("Status"), tr("Process")
                         };
    m_connectionsTable->setHorizontalHeaderLabels(headers);
    m_connectionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_connectionsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    connectionsLayout->addWidget(m_connectionsTable);
    
    // Add tabs
    tabWidget->addTab(appsTab, QIcon(":/png/apps.png"), tr("Applications"));
    tabWidget->addTab(connectionsTab, QIcon(":/png/connections.png"), tr("Connections"));
    
    mainLayout->addWidget(tabWidget);
    
    // Set central widget
    setCentralWidget(centralWidget);
    
    // Setup theme menu
    setupThemeMenu();
}

void MainWindow::setupMenuBar()
{
    // Create menu bar
    QMenuBar *menuBar = this->menuBar();
    
    // File menu
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));
    QAction *exportAction = new QAction(QIcon(":/icons/export.png"), tr("&Export Data..."), this);
    exportAction->setShortcut(QKeySequence::Save);
    exportAction->setStatusTip(tr("Export network data to file"));
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportData);
    fileMenu->addAction(exportAction);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);
    
    // View menu
    QMenu *viewMenu = menuBar->addMenu(tr("&View"));
    
    // Tools menu
    QMenu *toolsMenu = menuBar->addMenu(tr("&Tools"));
    QAction *firewallAction = toolsMenu->addAction(tr("&Firewall Rules..."));
    firewallAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_F));
    connect(firewallAction, &QAction::triggered, this, &MainWindow::showFirewallRules);
    toolsMenu->addAction(tr("View Alerts"), this, &MainWindow::showAlerts);
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("Alert Settings"), this, [this]() {
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
    });
    
    // Help menu
    QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        QMessageBox::about(this, tr("About NetWire"),
            tr("<h3>NetWire %1</h3>"
               "<p>A network monitoring tool with firewall capabilities.</p>"
               "<p>Version %1</p>"
               "<p>Copyright 2025 NetWire Team</p>")
            .arg(qApp->applicationVersion()));
    });
}

void MainWindow::setupConnections()
{
    // Connect network monitor signals to dashboard
    if (m_networkMonitor && m_dashboardWidget) {
        connect(m_networkMonitor, &NetworkMonitor::statsUpdated, 
                m_dashboardWidget, &DashboardWidget::updateBandwidthData);
        connect(m_networkMonitor, &NetworkMonitor::connectionCountChanged,
                m_dashboardWidget, &DashboardWidget::updateConnectionCount);
    }
    
    // Connect other signals
    connect(m_startStopAction, &QAction::triggered, this, &MainWindow::onStartStopClicked);
    connect(m_interfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onInterfaceSelected);
    connect(m_appsTable, &QTableView::customContextMenuRequested,
            this, &MainWindow::showContextMenu);
    connect(m_connectionsTable, &QTableWidget::customContextMenuRequested,
            this, &MainWindow::showContextMenu);
}

void MainWindow::setupToolbar()
{
    // Create toolbar
    QToolBar *toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->setMovable(false);
    
    // Start/Stop button
    m_startStopAction = new QAction(QIcon(":/png/start.png"), tr("Start"), this);
    m_startStopAction->setCheckable(true);
    toolbar->addAction(m_startStopAction);
    
    // Interface selection
    toolbar->addWidget(new QLabel(tr("Interface:"), this));
    m_interfaceCombo = new QComboBox(this);
    m_interfaceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    toolbar->addWidget(m_interfaceCombo);
    
    toolbar->addSeparator();
    
    // Refresh button
    QAction *refreshAction = new QAction(QIcon(":/png/refresh.png"), tr("Refresh"), this);
    toolbar->addAction(refreshAction);
    
    // Firewall button
    QAction *firewallAction = new QAction(QIcon(":/png/firewall.png"), tr("Firewall"), this);
    connect(firewallAction, &QAction::triggered, this, &MainWindow::showFirewallRules);
    toolbar->addAction(firewallAction);
    
    // Alerts button
    QAction *alertsAction = new QAction(QIcon(":/png/alert.png"), tr("Alerts"), this);
    connect(alertsAction, &QAction::triggered, this, &MainWindow::showAlerts);
    toolbar->addAction(alertsAction);
}

void MainWindow::setupSystemTray()
{
    // Create system tray icon
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icons/app.ico"));
    m_trayIcon->setToolTip(tr("NetWire - Network Monitor"));
    
    // Create tray menu
    m_trayMenu = new QMenu(this);
    
    // Show/Hide action
    m_showAction = new QAction(tr("Show"), this);
    connect(m_showAction, &QAction::triggered, this, &MainWindow::restoreFromTray);
    m_trayMenu->addAction(m_showAction);
    
    m_hideAction = new QAction(tr("Hide"), this);
    connect(m_hideAction, &QAction::triggered, this, &MainWindow::minimizeToTray);
    m_trayMenu->addAction(m_hideAction);
    
    m_trayMenu->addSeparator();
    
    // Start/Stop monitoring action
    QAction *startStopTrayAction = new QAction(tr("Start Monitoring"), this);
    startStopTrayAction->setCheckable(true);
    connect(startStopTrayAction, &QAction::triggered, this, &MainWindow::onStartStopClicked);
    m_trayMenu->addAction(startStopTrayAction);
    
    m_trayMenu->addSeparator();
    
    // Quit action
    m_quitAction = new QAction(tr("Quit"), this);
    connect(m_quitAction, &QAction::triggered, this, &MainWindow::quitApplication);
    m_trayMenu->addAction(m_quitAction);
    
    // Set tray menu
    m_trayIcon->setContextMenu(m_trayMenu);
    
    // Connect tray icon signals
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::trayIconActivated);
    
    // Show tray icon
    m_trayIcon->show();
}
    
    // Theme menu button
    QToolButton *themeButton = new QToolButton(this);
    themeButton->setIcon(QIcon(":/png/theme.png"));
    themeButton->setText(tr("Theme"));
    themeButton->setPopupMode(QToolButton::InstantPopup);
    themeButton->setMenu(new QMenu(this));
    toolbar->addWidget(themeButton);
    
    // Add theme actions to the theme button menu
    m_themeActionGroup = new QActionGroup(this);
    m_systemThemeAction = new QAction(tr("System"), this);
    m_systemThemeAction->setCheckable(true);
    m_lightThemeAction = new QAction(tr("Light"), this);
    m_lightThemeAction->setCheckable(true);
    m_darkThemeAction = new QAction(tr("Dark"), this);
    m_darkThemeAction->setCheckable(true);
    
    m_themeActionGroup->addAction(m_systemThemeAction);
    m_themeActionGroup->addAction(m_lightThemeAction);
    m_themeActionGroup->addAction(m_darkThemeAction);
    
    themeButton->menu()->addAction(m_systemThemeAction);
    themeButton->menu()->addAction(m_lightThemeAction);
    themeButton->menu()->addAction(m_darkThemeAction);
    
    // Connect theme actions
    connect(m_systemThemeAction, &QAction::triggered, this, [this]() { switchTheme(true, Theme::System); });
    connect(m_lightThemeAction, &QAction::triggered, this, [this]() { switchTheme(true, Theme::Light); });
    connect(m_darkThemeAction, &QAction::triggered, this, [this]() { switchTheme(true, Theme::Dark); });
}

void MainWindow::blockApplication()
{
    QModelIndexList selected = m_appsTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;
    
    QModelIndex sourceIndex = m_sortFilterModel->mapToSource(selected.first());
    QString appName = m_appModel->data(sourceIndex.sibling(sourceIndex.row(), 0)).toString();
    
    // Get the application path from the network monitor
    QString appPath = m_networkMonitor->getApplicationPath(appName);
    if (appPath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Could not determine application path."));
        return;
    }
    
    // Block the application using the firewall
    FirewallManager *firewall = FirewallManager::instance();
    QString ruleId = firewall->blockApplication(appPath, appName);
    
    if (ruleId.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), 
            tr("Failed to block application. Make sure you have administrator privileges."));
    } else {
        QMessageBox::information(this, tr("Application Blocked"), 
            tr("%1 has been blocked from accessing the network.").arg(appName));
    }
}

void MainWindow::showApplicationDetails()
{
    QModelIndexList selected = m_appsTable->selectionModel()->selectedRows();
    if (selected.isEmpty()) return;
    
    QModelIndex sourceIndex = m_sortFilterModel->mapToSource(selected.first());
    QString appName = m_appModel->data(sourceIndex.sibling(sourceIndex.row(), 0)).toString();
    
    // Get the application path from the network monitor
    QString appPath = m_networkMonitor->getApplicationPath(appName);
    
    // Get application information
    QFileInfo appInfo(appPath);
    QDateTime created = appInfo.birthTime();
    qint64 size = appInfo.size();
    
    // Get network statistics
    NetworkMonitor::NetworkStats stats = m_networkMonitor->getApplicationStats(appName);
    
    // Format details
    QString details;
    details += QString("<h3>%1</h3>").arg(appName);
    details += QString("<b>%1:</b> %2<br>").arg(tr("Path")).arg(appPath);
    
    if (created.isValid()) {
        details += QString("<b>%1:</b> %2<br>")
            .arg(tr("Created"))
            .arg(created.toString(Qt::SystemLocaleLongDate));
    }
    
    details += QString("<b>%1:</b> %2<br>")
        .arg(tr("Size"))
        .arg(formatBytes(size));
    
    details += "<br><b>" + tr("Network Activity") + "</b><br>";
    details += QString("<b>%1:</b> %2<br>")
        .arg(tr("Downloaded"))
        .arg(formatBytes(stats.downloaded));
    
    details += QString("<b>%1:</b> %2<br>")
        .arg(tr("Uploaded"))
        .arg(formatBytes(stats.uploaded));
    
    // Check if application is blocked
    FirewallManager *firewall = FirewallManager::instance();
    bool isBlocked = !appPath.isEmpty() && firewall->isAppBlocked(appPath);
    
    details += QString("<b>%1:</b> %2<br>")
        .arg(tr("Status"))
        .arg(isBlocked ? tr("Blocked") : tr("Allowed"));
    
    // Show details in a message box
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Application Details"));
    msgBox.setText(details);
    
    // Add buttons based on blocked status
    QPushButton *toggleBlockBtn = nullptr;
    if (isBlocked) {
        toggleBlockBtn = msgBox.addButton(tr("Allow Application"), QMessageBox::ActionRole);
    } else if (!appPath.isEmpty()) {
        toggleBlockBtn = msgBox.addButton(tr("Block Application"), QMessageBox::ActionRole);
    }
    
    msgBox.addButton(QMessageBox::Close);
    msgBox.exec();
    
    // Handle button clicks
    if (msgBox.clickedButton() == toggleBlockBtn) {
        if (isBlocked) {
            // Find and remove the block rule
            const auto rules = firewall->rules();
            for (const auto &rule : rules) {
                if ((rule.type == FirewallManager::BlockApp || rule.type == FirewallManager::AllowApp) && 
                    QFileInfo(rule.appPath).canonicalFilePath() == QFileInfo(appPath).canonicalFilePath()) {
                    firewall->removeRule(rule.id);
                    break;
                }
            }
        } else {
            // Add a block rule
            firewall->blockApplication(appPath, appName);
        }
    }
}

void MainWindow::showFirewallRules()
{
    if (!m_firewallDialog) {
        m_firewallDialog = new FirewallRulesDialog(this);
    }
    
    m_firewallDialog->show();
    m_firewallDialog->raise();
    m_firewallDialog->activateWindow();
}

void MainWindow::setMonitoring(bool enabled)
{
    m_isMonitoring = enabled;
    
    // Update UI
    m_startStopAction->setChecked(enabled);
    m_startStopAction->setIcon(QIcon(
        enabled ? ":/icons/stop.png" : ":/icons/start.png"));
    m_startStopAction->setText(enabled ? tr("Stop") : tr("Start"));
    
    m_interfaceCombo->setEnabled(!enabled);
    m_refreshAction->setEnabled(!enabled);
    
    // Update status bar
    if (enabled) {
        m_statusLabel->setText(tr("Monitoring %1...").arg(m_interfaceCombo->currentText()));
    } else {
        m_statusLabel->setText(tr("Stopped"));
    }
}

// System tray slot implementations
void MainWindow::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        restoreFromTray();
    }
}

void MainWindow::showTrayMenu()
{
    if (m_trayMenu) {
        m_trayMenu->popup(QCursor::pos());
    }
}

void MainWindow::minimizeToTray()
{
    hide();
    if (m_trayIcon) {
        m_trayIcon->showMessage(tr("NetWire"), tr("Minimized to system tray"), 
                               QSystemTrayIcon::Information, 2000);
    }
}

void MainWindow::restoreFromTray()
{
    show();
    raise();
    activateWindow();
}

void MainWindow::quitApplication()
{
    // Stop monitoring if active
    if (m_isMonitoring) {
        setMonitoring(false);
    }
    
    // Close the application
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Check if system tray is available
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        // Minimize to tray instead of closing
        minimizeToTray();
        event->ignore();
    } else {
        // If no system tray, close normally
        event->accept();
    }
}
