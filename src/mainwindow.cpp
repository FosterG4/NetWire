#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "globallogger.h"
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QSettings>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTextStream>
#include <QMessageBox>
#include <QApplication>
#include "alertmanager.h"
#include "alertsettingsdialog.h"
#include <QMessageBox>
#include <QMenu>
#include <QFileDialog>
#include <QPushButton>
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
#include <QIcon>

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
        return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
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
        appData.icon = stat.processIcon.isNull() ? QIcon(":/resources/icons/application.ico") : stat.processIcon;
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
    , m_alertManager(AlertManager::instance())
    , m_alertsDialog(new AlertsDialog(this))
    , m_isMonitoring(false)
{
    LOG_FUNCTION_ENTRY();
    
    ui->setupUi(this);
    
    LOG_DEBUG("Setting window properties");
    setWindowTitle(tr("NetWire - Network Monitor"));
    resize(1200, 600); // More horizontal, less vertical
    
    LOG_DEBUG("About to call setupUI()");
    setupUI();
    
    LOG_DEBUG("About to call setupConnections()");
    setupConnections();
}

MainWindow::~MainWindow()
{
    // Stop monitoring if active
    if (m_isMonitoring) {
        setMonitoring(false);
        if (m_networkMonitor) {
            m_networkMonitor->stopCapture();
        }
    }
    
    // Stop AlertManager monitoring
    if (m_alertManager) {
        m_alertManager->stopMonitoring();
    }
    
    // Save window state and geometry
    QSettings settings("NetWire", "NetWire");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    
    delete ui;
}

void MainWindow::setupUI()
{
    LOG_FUNCTION_ENTRY();
    
    m_alertsDialog->setAlertManager(m_alertManager);
    
    setWindowTitle(tr("NetWire - Network Monitor"));
    resize(1400, 500); // Much more horizontal, less vertical
    
    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(1, 1, 1, 1); // Minimal margins
    mainLayout->setSpacing(0); // No spacing
    
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setLayout(mainLayout);
    setCentralWidget(centralWidget);
    
    setupToolbar();
    
    // Create the main tab widget
    QTabWidget *tabWidget = new QTabWidget(this);
    tabWidget->setTabPosition(QTabWidget::North);
    tabWidget->setTabBarAutoHide(false);
    tabWidget->setDocumentMode(true);
    tabWidget->setMinimumHeight(400); // Smaller minimum height
    mainLayout->addWidget(tabWidget);
    
    // Create dashboard tab
    m_dashboardWidget = new DashboardWidget(this);
    tabWidget->addTab(m_dashboardWidget, tr("Dashboard"));
    
    // Create network activity heatmap tab
    QWidget *heatmapTab = new QWidget(this);
    QVBoxLayout *heatmapLayout = new QVBoxLayout(heatmapTab);
    heatmapLayout->setContentsMargins(1, 1, 1, 1); // Minimal margins
    heatmapLayout->setSpacing(1); // Minimal spacing
    
    // Create compact toolbar for heatmap controls
    QToolBar *heatmapToolbar = new QToolBar(tr("Heatmap Controls"), this);
    heatmapToolbar->setIconSize(QSize(20, 20)); // Standard icon size
    heatmapToolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    heatmapToolbar->setMovable(false);
    heatmapToolbar->setMaximumHeight(40); // Standard height
    
    // Add time range controls
    QLabel *rangeLabel = new QLabel(tr("Range:"), this);
    rangeLabel->setMinimumWidth(50);
    heatmapToolbar->addWidget(rangeLabel);
    
    QComboBox *rangeCombo = new QComboBox(this);
    rangeCombo->setMaximumWidth(120); // Standard width
    rangeCombo->addItem(tr("7 Days"), 7);
    rangeCombo->addItem(tr("14 Days"), 14);
    rangeCombo->addItem(tr("30 Days"), 30);
    rangeCombo->addItem(tr("3 Months"), 90);
    rangeCombo->addItem(tr("6 Months"), 180);
    rangeCombo->addItem(tr("1 Year"), 365);
    rangeCombo->setCurrentIndex(0);
    
    // Connect range changed signal
    connect(rangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, rangeCombo](int index) {
        Q_UNUSED(index);
        int days = rangeCombo->currentData().toInt();
        QDateTime now = QDateTime::currentDateTime();
        QDateTime startTime = now.addDays(-days + 1);
        m_networkHeatmap->setTimeRange(startTime, now);
    });
    
    heatmapToolbar->addWidget(rangeCombo);
    heatmapToolbar->addSeparator();
    
    // Create network heatmap first
    m_networkHeatmap = new NetworkHeatmap(this);
    m_networkHeatmap->setXAxisLabel(tr("Day of Week"));
    m_networkHeatmap->setYAxisLabel(tr("Time of Day"));
    m_networkHeatmap->setLegendVisible(true);
    
    // Set initial time range (last 7 days)
    QDateTime now = QDateTime::currentDateTime();
    QDateTime startTime = now.addDays(-6);
    m_networkHeatmap->setTimeRange(startTime, now);
    
    // Add very compact data visibility toggles
    QCheckBox *showDownloadCheck = new QCheckBox(tr("Show Download"), this);
    showDownloadCheck->setChecked(true);
    showDownloadCheck->setMaximumWidth(100);
    connect(showDownloadCheck, &QCheckBox::toggled, 
            m_networkHeatmap, &NetworkHeatmap::setShowDownloadData);
    heatmapToolbar->addWidget(showDownloadCheck);
    
    QCheckBox *showUploadCheck = new QCheckBox(tr("Show Upload"), this);
    showUploadCheck->setChecked(true);
    showUploadCheck->setMaximumWidth(90);
    connect(showUploadCheck, &QCheckBox::toggled, 
            m_networkHeatmap, &NetworkHeatmap::setShowUploadData);
    heatmapToolbar->addWidget(showUploadCheck);
    
    QCheckBox *showCombinedCheck = new QCheckBox(tr("Show Combined"), this);
    showCombinedCheck->setChecked(true);
    showCombinedCheck->setMaximumWidth(100);
    connect(showCombinedCheck, &QCheckBox::toggled, 
            m_networkHeatmap, &NetworkHeatmap::setShowCombinedData);
    heatmapToolbar->addWidget(showCombinedCheck);
    
    // Add compact export button
    QToolButton *exportButton = new QToolButton(this);
    exportButton->setText(tr("Export"));
    exportButton->setIcon(QIcon(":/resources/icons/png/save.png"));
    exportButton->setToolTip(tr("Export heatmap data"));
    exportButton->setPopupMode(QToolButton::MenuButtonPopup);
    exportButton->setMaximumWidth(80);
    
    QMenu *exportMenu = new QMenu(exportButton);
    QAction *exportCsvAction = exportMenu->addAction(tr("Export to CSV..."));
    QAction *exportPngAction = exportMenu->addAction(tr("Export to PNG..."));
    exportButton->setMenu(exportMenu);
    
    // Export connections
    connect(exportCsvAction, &QAction::triggered, this, [this]() {
        QString fileName = QFileDialog::getSaveFileName(this,
            tr("Export Heatmap Data"),
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/network_activity.csv",
            tr("CSV Files (*.csv)"));
            
        if (!fileName.isEmpty()) {
            if (!fileName.endsWith(".csv", Qt::CaseInsensitive)) {
                fileName += ".csv";
            }
            
            if (m_networkHeatmap->exportToCsv(fileName)) {
                QMessageBox::information(this, tr("Export Successful"),
                    tr("Heatmap data has been exported to:\n%1").arg(fileName));
            } else {
                QMessageBox::warning(this, tr("Export Failed"),
                    tr("Failed to export heatmap data to:\n%1").arg(fileName));
            }
        }
    });
    
    connect(exportPngAction, &QAction::triggered, this, [this]() {
        QString fileName = QFileDialog::getSaveFileName(this,
            tr("Export Heatmap Image"),
            QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/network_activity.png",
            tr("PNG Images (*.png)"));
            
        if (!fileName.isEmpty()) {
            if (!fileName.endsWith(".png", Qt::CaseInsensitive)) {
                fileName += ".png";
            }
            
            if (m_networkHeatmap->exportToImage(fileName, "PNG")) {
                QMessageBox::information(this, tr("Export Successful"),
                    tr("Heatmap image has been exported to:\n%1").arg(fileName));
            } else {
                QMessageBox::warning(this, tr("Export Failed"),
                    tr("Failed to export heatmap image to:\n%1").arg(fileName));
            }
        }
    });
    
    heatmapToolbar->addWidget(exportButton);
    
    // Add widgets to layout
    heatmapLayout->addWidget(heatmapToolbar);
    heatmapLayout->addWidget(m_networkHeatmap, 1);
    tabWidget->addTab(heatmapTab, tr("Network Activity"));
    
    // Create applications tab
    QWidget *appsTab = new QWidget(this);
    QVBoxLayout *appsLayout = new QVBoxLayout(appsTab);
    appsLayout->setContentsMargins(1, 1, 1, 1); // Minimal margins
    appsLayout->setSpacing(0); // No spacing
    
    // Create scroll area for applications table
    QScrollArea *appsScrollArea = new QScrollArea(this);
    appsScrollArea->setWidgetResizable(true);
    appsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    appsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    appsScrollArea->setMaximumHeight(350); // Limit height
    
    // Create applications table
    m_appsTable = new QTableView(this);
    m_appsTable->setModel(m_appModel);
    m_appsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_appsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_appsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_appsTable->setSortingEnabled(true);
    m_appsTable->sortByColumn(0, Qt::AscendingOrder);
    m_appsTable->setAlternatingRowColors(true);
    m_appsTable->setShowGrid(true);
    m_appsTable->setGridStyle(Qt::SolidLine);
    m_appsTable->setMinimumHeight(300);
    
    // Setup table columns with better proportions
    m_appsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_appsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_appsTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_appsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    
    // Set specific column widths
    m_appsTable->setColumnWidth(0, 150); // Application name
    m_appsTable->setColumnWidth(2, 80);  // Download
    m_appsTable->setColumnWidth(3, 80);  // Upload
    
    appsScrollArea->setWidget(m_appsTable);
    appsLayout->addWidget(appsScrollArea);
    tabWidget->addTab(appsTab, tr("Applications"));
    
    // Create connections tab
    QWidget *connectionsTab = new QWidget();
    QVBoxLayout *connectionsLayout = new QVBoxLayout(connectionsTab);
    connectionsLayout->setContentsMargins(1, 1, 1, 1); // Minimal margins
    connectionsLayout->setSpacing(0); // No spacing
    
    // Create scroll area for connections table
    QScrollArea *connectionsScrollArea = new QScrollArea(this);
    connectionsScrollArea->setWidgetResizable(true);
    connectionsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    connectionsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    connectionsScrollArea->setMaximumHeight(350); // Limit height
    
    // Create connections table
    m_connectionsTable = new QTableWidget(0, 5, this);
    
    QStringList headers;
    headers << tr("Protocol") << tr("Local Address") << tr("Remote Address") << tr("Status") << tr("Process");
    m_connectionsTable->setHorizontalHeaderLabels(headers);
    m_connectionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_connectionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_connectionsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    m_connectionsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_connectionsTable->verticalHeader()->setVisible(false);
    m_connectionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_connectionsTable->setAlternatingRowColors(true);
    m_connectionsTable->setShowGrid(true);
    m_connectionsTable->setGridStyle(Qt::SolidLine);
    m_connectionsTable->setMinimumHeight(300);
    
    // Set initial column widths for better space usage
    m_connectionsTable->setColumnWidth(0, 60);  // Protocol
    m_connectionsTable->setColumnWidth(1, 150); // Local Address
    m_connectionsTable->setColumnWidth(2, 150); // Remote Address
    m_connectionsTable->setColumnWidth(3, 60);  // Status
    m_connectionsTable->setColumnWidth(4, 100); // Process
    
    connectionsScrollArea->setWidget(m_connectionsTable);
    connectionsLayout->addWidget(connectionsScrollArea);
    tabWidget->addTab(connectionsTab, tr("Connections"));
    
    // Create status bar
    m_statusLabel = new QLabel(tr("Ready"), this);
    statusBar()->addPermanentWidget(m_statusLabel, 1);
    statusBar()->setMaximumHeight(16); // Very compact status bar
    
    setupSystemTray();
    
    setupMenuBar();
    
    setupConnections();
    
    loadTheme();
    
    // Update interface list
    updateInterfaceList();
}

// This function sets up all signal-slot connections for the MainWindow class
void MainWindow::setupConnections()
{
    LOG_FUNCTION_ENTRY();
    
    // 1. Connect toolbar actions
    connect(m_startStopAction, &QAction::triggered, this, &MainWindow::onStartStopClicked);
    connect(m_refreshAction, &QAction::triggered, this, &MainWindow::updateInterfaceList);
    
    // 2. Connect interface selection
    if (m_interfaceCombo) {
        connect(m_interfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MainWindow::onInterfaceSelected);
    }
    
    // 3. Connect context menu signals
    if (m_appsTable) {
        connect(m_appsTable, &QTableView::customContextMenuRequested,
                this, &MainWindow::showContextMenu);
    }
    
    // 4. Connect network monitor signals
    if (m_networkMonitor) {
        // Connect to dashboard widgets
        if (m_dashboardWidget) {
            connect(m_networkMonitor, &NetworkMonitor::statsUpdated, 
                    m_dashboardWidget, &DashboardWidget::updateBandwidthData);
            connect(m_networkMonitor, &NetworkMonitor::statsUpdated, 
                    m_dashboardWidget, &DashboardWidget::updateConnectionCount);
            connect(m_networkMonitor, &NetworkMonitor::statsUpdated, 
                    m_dashboardWidget, &DashboardWidget::updateSystemResources);
        }
        
        // Connect to main window stats update
        connect(m_networkMonitor, &NetworkMonitor::statsUpdated,
                this, &MainWindow::updateNetworkStats);
        
        // Connect to update heatmap (throttled to every 5 minutes)
        connect(m_networkMonitor, &NetworkMonitor::statsUpdated, this, 
            [this](quint64 download, quint64 upload) {
                static QDateTime lastUpdate = QDateTime::currentDateTime();
                QDateTime now = QDateTime::currentDateTime();
                
                if (lastUpdate.secsTo(now) >= 300) {  // 5 minutes
                    lastUpdate = now;
                        
                        // Add data point to heatmap if available
                        if (m_networkHeatmap) {
                            m_networkHeatmap->addDataPoint(now, download, false);
                            m_networkHeatmap->addDataPoint(now, upload, true);
                        }
                    }
                });
        }
    
    // 5. Connect alert manager signals
    if (m_alertManager) {
        connect(m_alertManager, &AlertManager::newAlert,
                this, &MainWindow::onAlertReceived);
    }
    
    // 6. Connect theme actions
    if (m_systemThemeAction) {
        connect(m_systemThemeAction, &QAction::triggered, 
                this, [this]() { switchTheme(true, Theme::System); });
    }
    
    if (m_lightThemeAction) {
        connect(m_lightThemeAction, &QAction::triggered, 
                this, [this]() { switchTheme(true, Theme::Light); });
    }
    
    if (m_darkThemeAction) {
        connect(m_darkThemeAction, &QAction::triggered, 
                this, [this]() { switchTheme(true, Theme::Dark); });
    }
    
    // 7. Connect system tray signals
    if (m_trayIcon) {
        connect(m_trayIcon, &QSystemTrayIcon::activated,
                this, &MainWindow::trayIconActivated);
    }
    
    // 8. Connect connections table context menu
    if (m_connectionsTable) {
        connect(m_connectionsTable, &QTableWidget::customContextMenuRequested,
                this, &MainWindow::showContextMenu);
    }
}

void MainWindow::setupToolbar()
{
    LOG_FUNCTION_ENTRY();
    // Create toolbar
    QToolBar *toolbar = addToolBar(tr("Main Toolbar"));
    toolbar->setMovable(false);
    toolbar->setIconSize(QSize(24, 24)); // Standard icon size
    toolbar->setMaximumHeight(45); // Standard toolbar height
    
    // Start/Stop button
    m_startStopAction = new QAction(QIcon(":/png/start.png"), tr("Start"), this);
    m_startStopAction->setCheckable(true);
    toolbar->addAction(m_startStopAction);
    
    // Interface selection
    QLabel *interfaceLabel = new QLabel(tr("Interface:"), this);
    interfaceLabel->setMaximumWidth(80);
    toolbar->addWidget(interfaceLabel);
    m_interfaceCombo = new QComboBox(this);
    m_interfaceCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_interfaceCombo->setMaximumWidth(200); // Standard width
    toolbar->addWidget(m_interfaceCombo);
    
    toolbar->addSeparator();
    
    // Refresh button
    m_refreshAction = new QAction(tr("Refresh"), this);
    if (QFile::exists(":/resources/icons/png/refresh.png")) {
        m_refreshAction->setIcon(QIcon(":/resources/icons/png/refresh.png"));
    } else if (QFile::exists(":/png/refresh.png")) {
        m_refreshAction->setIcon(QIcon(":/png/refresh.png"));
    }
    toolbar->addAction(m_refreshAction);
    
    // Firewall button
    QAction *firewallAction = new QAction(tr("Firewall"), this);
    if (QFile::exists(":/resources/icons/png/connections.png")) {
        firewallAction->setIcon(QIcon(":/resources/icons/png/connections.png"));
    } else if (QFile::exists(":/png/connections.png")) {
        firewallAction->setIcon(QIcon(":/png/connections.png"));
    }
    connect(firewallAction, &QAction::triggered, this, &MainWindow::showFirewallRules);
    toolbar->addAction(firewallAction);
    
    // Alerts button
    QAction *alertsAction = new QAction(tr("Alerts"), this);
    if (QFile::exists(":/resources/icons/png/theme.png")) {
        alertsAction->setIcon(QIcon(":/resources/icons/png/theme.png"));
    } else if (QFile::exists(":/png/theme.png")) {
        alertsAction->setIcon(QIcon(":/png/theme.png"));
    }
    connect(alertsAction, &QAction::triggered, this, &MainWindow::showAlerts);
    toolbar->addAction(alertsAction);
    
    // Add a separator
    toolbar->addSeparator();
    
    // Test Alerts button (only in debug builds)
#ifndef QT_NO_DEBUG
    QAction *testAlertsAction = new QAction(QIcon(":/png/test.png"), tr("Test Alerts"), this);
    testAlertsAction->setToolTip(tr("Generate test alerts for demonstration"));
    connect(testAlertsAction, &QAction::triggered, this, &MainWindow::testAlertScenarios);
    toolbar->addAction(testAlertsAction);
#endif
}

void MainWindow::setupSystemTray()
{
    LOG_FUNCTION_ENTRY();
    // Create system tray icon
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/resources/icons/app.ico"));
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
            .arg(created.toString(QLocale::system().dateFormat(QLocale::LongFormat)));
    }
    
    details += QString("<b>%1:</b> %2<br>")
        .arg(tr("Size"))
        .arg(formatBytes(size));
    
    details += "<br><b>" + tr("Network Activity") + "</b><br>";
    details += QString("<b>%1:</b> %2<br>")
        .arg(tr("Downloaded"))
        .arg(formatBytes(stats.totalDownloaded));
    
    details += QString("<b>%1:</b> %2<br>")
        .arg(tr("Uploaded"))
        .arg(formatBytes(stats.totalUploaded));
    
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

void MainWindow::exportData()
{
    // Get the default documents folder
    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    
    // Create a filename with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString defaultFileName = QString("%1/netwire_export_%2.csv").arg(defaultPath).arg(timestamp);
    
    // Open file dialog to select save location
    QString fileName = QFileDialog::getSaveFileName(this, 
        tr("Export Network Data"), 
        defaultFileName,
        tr("CSV Files (*.csv);;All Files (*)"));
    
    if (fileName.isEmpty()) {
        return; // User cancelled
    }
    
    // Ensure the file has a .csv extension
    if (!fileName.endsWith(".csv", Qt::CaseInsensitive)) {
        fileName += ".csv";
    }
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Export Error"), 
            tr("Could not open file for writing: %1").arg(file.errorString()));
        return;
    }
    
    QTextStream out(&file);
    
    // Write CSV header
    out << "Timestamp,Application,Download (B),Upload (B),Total (B),Connections\n";
    
    // Get current timestamp for the export
    QString timestampStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    
    // Write data for each application
    for (int row = 0; row < m_appModel->rowCount(); ++row) {
        QString appName = m_appModel->data(m_appModel->index(row, 0), Qt::DisplayRole).toString();
        QString download = m_appModel->data(m_appModel->index(row, 1), Qt::DisplayRole).toString();
        QString upload = m_appModel->data(m_appModel->index(row, 2), Qt::DisplayRole).toString();
        QString total = m_appModel->data(m_appModel->index(row, 3), Qt::DisplayRole).toString();
        QString connections = m_appModel->data(m_appModel->index(row, 4), Qt::DisplayRole).toString();
        
        // Convert formatted strings back to raw numbers for CSV
        // This is a simplified approach - in a real app, you'd want to store the raw numbers
        // and only format them for display in the UI
        download = download.split(" ").first(); // Remove units
        upload = upload.split(" ").first();
        total = total.split(" ").first();
        
        // Write CSV line
        out << QString("%1,%2,%3,%4,%5,%6\n")
            .arg(timestampStr)
            .arg("\"" + appName + "\"") // Quote app name in case it contains commas
            .arg(download)
            .arg(upload)
            .arg(total)
            .arg(connections);
    }
    
    file.close();
    
    // Show success message
    QMessageBox::information(this, tr("Export Complete"), 
        tr("Network data has been exported to:\n%1").arg(fileName));
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

void MainWindow::showAlerts()
{
    if (!m_alertsDialog) {
        m_alertsDialog = new AlertsDialog(this);
        if (m_alertManager) {
            m_alertsDialog->setAlertManager(m_alertManager);
        }
    }
    
    m_alertsDialog->show();
    m_alertsDialog->raise();
    m_alertsDialog->activateWindow();
    
    // Refresh the alerts when the dialog is shown
    m_alertsDialog->updateAlerts();
}

void MainWindow::testAlertScenarios()
{
    if (!m_alertManager) {
        qWarning() << "Alert manager not available for testing";
        return;
    }

    // Get current timestamp
    QDateTime now = QDateTime::currentDateTime();
    
    // Test 1: New Application Detected (Info)
    {
        AlertManager::Alert alert;
        alert.type = AlertManager::NewAppDetected;
        alert.severity = AlertManager::Info;
        alert.title = tr("New Application Detected");
        alert.description = tr("A new application 'TestApp.exe' was detected on the network");
        alert.source = "192.168.1.100:54321";
        alert.destination = "93.184.216.34:443";
        alert.bytesTransferred = 1024;
        alert.timestamp = now.addSecs(-300); // 5 minutes ago
        alert.acknowledged = false;
        alert.additionalInfo = "Process ID: 1234\nPath: C:\\Program Files\\TestApp\\testapp.exe";
        
        emit m_alertManager->newAlert(alert);
    }
    
    // Test 2: High Bandwidth Usage (Warning)
    {
        AlertManager::Alert alert;
        alert.type = AlertManager::HighBandwidthUsage;
        alert.severity = AlertManager::Medium;
        alert.title = tr("High Bandwidth Usage");
        alert.description = tr("Application 'TestApp.exe' is using 15.7 MB/s of bandwidth");
        alert.source = "192.168.1.100:54322";
        alert.destination = "151.101.1.69:443";
        alert.bytesTransferred = 15728640; // 15 MB
        alert.timestamp = now.addSecs(-180); // 3 minutes ago
        alert.acknowledged = false;
        alert.additionalInfo = "Threshold: 10.0 MB/s\nDuration: 2m 30s";
        
        emit m_alertManager->newAlert(alert);
    }
    
    // Test 3: Suspicious Connection (High)
    {
        AlertManager::Alert alert;
        alert.type = AlertManager::SuspiciousConnection;
        alert.severity = AlertManager::High;
        alert.title = tr("Suspicious Connection Detected");
        alert.description = tr("Connection to known malicious IP 185.130.5.253");
        alert.source = "192.168.1.100:54323";
        alert.destination = "185.130.5.253:4444";
        alert.bytesTransferred = 5242880; // 5 MB
        alert.timestamp = now.addSecs(-120); // 2 minutes ago
        alert.acknowledged = false;
        alert.additionalInfo = "Threat: Cobalt Strike C2\nConfidence: 95%\nFirst Seen: 2023-01-15";
        
        emit m_alertManager->newAlert(alert);
    }
    
    // Test 4: Data Exfiltration (Critical)
    {
        AlertManager::Alert alert;
        alert.type = AlertManager::DataExfiltration;
        alert.severity = AlertManager::Critical;
        alert.title = tr("Possible Data Exfiltration");
        alert.description = tr("Large amount of data (42.5 MB) being sent to external server");
        alert.source = "192.168.1.100:54324";
        alert.destination = "45.227.253.108:8080";
        alert.bytesTransferred = 44564480; // 42.5 MB
        alert.timestamp = now.addSecs(-60); // 1 minute ago
        alert.acknowledged = false;
        alert.additionalInfo = "File type: ZIP archive\nSuspicious pattern: Credit card numbers detected";
        
        emit m_alertManager->newAlert(alert);
    }
    
    // Show a message that test alerts were generated
    m_statusLabel->setText(tr("Generated test alerts - check Alerts window"));
    QTimer::singleShot(5000, this, [this]() {
        if (m_statusLabel) {
            m_statusLabel->clear();
        }
    });
}

void MainWindow::updateInterfaceList()
{
    DEBUG_WRAP_FUNCTION();
    if (!m_interfaceCombo) {
        return;
    }

    // Store current selection
    QString currentInterface = m_interfaceCombo->currentData().toString();
    
    // Clear the combo box
    m_interfaceCombo->clear();
    
    // Get all available network interfaces
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    
    // Add a default "All Interfaces" option
    m_interfaceCombo->addItem(tr("All Interfaces"), "");
    
    // Add each interface to the combo box
    for (const QNetworkInterface &netInterface : interfaces) {
        // Skip loopback and inactive interfaces
        if (netInterface.flags() & QNetworkInterface::IsLoopBack || 
            !(netInterface.flags() & QNetworkInterface::IsUp) ||
            !(netInterface.flags() & QNetworkInterface::IsRunning)) {
            continue;
        }
        
        QString displayName = netInterface.humanReadableName();
        if (displayName.isEmpty()) {
            displayName = netInterface.name();
        }
        
        // Add the interface to the combo box
        m_interfaceCombo->addItem(displayName, netInterface.name());
    }
    
    // Restore previous selection if it still exists
    int index = m_interfaceCombo->findData(currentInterface);
    if (index >= 0) {
        m_interfaceCombo->setCurrentIndex(index);
    } else if (m_interfaceCombo->count() > 0) {
        // Select the first interface by default
        m_interfaceCombo->setCurrentIndex(0);
    }
}

void MainWindow::onAlertReceived(const AlertManager::Alert &alert)
{
    // Update the status bar with the alert message
    if (m_statusLabel) {
        QString alertText = QString("ALERT: %1 - %2")
            .arg(alert.title)
            .arg(alert.description.left(100)); // Limit description length
        m_statusLabel->setText(alertText);
        
        // Set text color based on alert severity
        QPalette palette = m_statusLabel->palette();
        switch (alert.severity) {
            case AlertManager::Info:
                palette.setColor(QPalette::WindowText, Qt::blue);
                break;
            case AlertManager::Low:
                palette.setColor(QPalette::WindowText, Qt::darkGreen);
                break;
            case AlertManager::Medium:
                palette.setColor(QPalette::WindowText, QColor(255, 165, 0)); // Orange
                break;
            case AlertManager::High:
                palette.setColor(QPalette::WindowText, QColor(255, 69, 0)); // OrangeRed
                break;
            case AlertManager::Critical:
                palette.setColor(QPalette::WindowText, Qt::red);
                break;
            default:
                palette.setColor(QPalette::WindowText, Qt::black);
                break;
        }
        m_statusLabel->setPalette(palette);
        
        // Show the status bar if it was hidden
        statusBar()->show();
        
        // Reset the status bar after 10 seconds
        QTimer::singleShot(10000, this, [this]() {
            if (m_statusLabel) {
                m_statusLabel->clear();
                m_statusLabel->setPalette(QPalette()); // Reset to default palette
            }
        });
    }
    
    // If the alerts dialog is visible, update it
    if (m_alertsDialog && m_alertsDialog->isVisible()) {
        m_alertsDialog->updateAlerts();
    }
    
    // Show a system tray message if minimized
    if (m_trayIcon && isMinimized()) {
        m_trayIcon->showMessage(
            alert.title,
            alert.description,
            QSystemTrayIcon::Information,
            5000 // 5 seconds
        );
    }
}

void MainWindow::setMonitoring(bool enabled)
{
    if (m_isMonitoring == enabled) {
        return; // No change needed
    }
    
    m_isMonitoring = enabled;
    
    // Update UI elements
    if (m_startStopAction) {
        m_startStopAction->setChecked(enabled);
        m_startStopAction->setIcon(QIcon(enabled ? ":/resources/icons/png/stop.png" : ":/resources/icons/png/start.png"));
        m_startStopAction->setText(enabled ? tr("Stop") : tr("Start"));
    }
    
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
        if (m_networkMonitor) {
            m_networkMonitor->stopCapture();
        }
    }
    
    // Stop AlertManager monitoring
    if (m_alertManager) {
        m_alertManager->stopMonitoring();
    }
    
    // Close the application
    QApplication::quit();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Stop monitoring if active
    if (m_isMonitoring) {
        setMonitoring(false);
        if (m_networkMonitor) {
            m_networkMonitor->stopCapture();
        }
    }
    
    // Stop AlertManager monitoring
    if (m_alertManager) {
        m_alertManager->stopMonitoring();
    }
    
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

// Missing function implementations
void MainWindow::updateNetworkStats()
{
    if (!m_networkMonitor) return;
    
    auto stats = m_networkMonitor->getStats();
    quint64 totalDownload = 0;
    quint64 totalUpload = 0;
    
    for (const auto &stat : stats) {
        totalDownload += stat.totalDownloaded;
        totalUpload += stat.totalUploaded;
    }
    
    // Update status bar
    if (m_statusLabel) {
        QString statusText = tr("Download: %1 | Upload: %2")
            .arg(formatBytes(totalDownload))
            .arg(formatBytes(totalUpload));
        m_statusLabel->setText(statusText);
    }
    
    // Update charts if available
    if (m_dashboardWidget) {
        m_dashboardWidget->updateBandwidthData(totalDownload, totalUpload);
    }
}

void MainWindow::onStartStopClicked()
{
    if (m_isMonitoring) {
        setMonitoring(false);
        if (m_networkMonitor) {
            m_networkMonitor->stopCapture();
        }
    } else {
        setMonitoring(true);
        if (m_networkMonitor) {
            QString interfaceName = m_interfaceCombo->currentData().toString();
            m_networkMonitor->startCapture(interfaceName);
        }
    }
}

void MainWindow::onInterfaceSelected(int index)
{
    if (index < 0) return;
    
    QString interfaceName = m_interfaceCombo->itemData(index).toString();
    
    // Stop current monitoring if active
    if (m_isMonitoring && m_networkMonitor) {
        m_networkMonitor->stopCapture();
    }
    
    // Start monitoring on new interface if monitoring is enabled
    if (m_isMonitoring && m_networkMonitor) {
        m_networkMonitor->startCapture(interfaceName);
    }
    
    // Update status
    if (m_statusLabel) {
        m_statusLabel->setText(tr("Interface: %1").arg(interfaceName));
    }
}

void MainWindow::showContextMenu(const QPoint &pos)
{
    QMenu contextMenu(this);
    
    // Add actions based on context
    QAction *detailsAction = contextMenu.addAction(tr("Show Details"));
    QAction *blockAction = contextMenu.addAction(tr("Block Application"));
    QAction *exportAction = contextMenu.addAction(tr("Export Data"));
    
    // Show menu and handle selection
    QAction *selectedAction = contextMenu.exec(pos);
    if (!selectedAction) return;
    
    if (selectedAction == detailsAction) {
        showApplicationDetails();
    } else if (selectedAction == blockAction) {
        blockApplication();
    } else if (selectedAction == exportAction) {
        exportData();
    }
}

void MainWindow::switchTheme(bool enabled, Theme theme)
{
    if (!enabled) return;
    
    m_currentTheme = theme;
    updateTheme();
    
    // Save theme preference
    QSettings settings;
    settings.setValue("theme", static_cast<int>(theme));
}

void MainWindow::updateTheme()
{
    DEBUG_WRAP_FUNCTION();
    
    // Load the stylesheet file
    QFile styleFile(":/resources/style.qss");
    if (!styleFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open stylesheet file";
        return;
    }
    
    QString styleSheet = QLatin1String(styleFile.readAll());
    
    // Apply theme-specific modifications
    if (m_currentTheme == Theme::Dark) {
        // Set dark theme property
        setProperty("theme", "dark");
        styleSheet += "\nQMainWindow[theme=\"dark\"] { background-color: #2d2d2d; color: #f0f0f0; }";
    } else if (m_currentTheme == Theme::Light) {
        // Set light theme property
        setProperty("theme", "light");
        styleSheet += "\nQMainWindow[theme=\"light\"] { background-color: #ffffff; color: #2c2c2c; }";
    } else {
        // System theme - use default
        setProperty("theme", "system");
    }
    
    // Apply the stylesheet
    setStyleSheet(styleSheet);
    
    // Force style update
    style()->unpolish(this);
    style()->polish(this);
}

void MainWindow::loadTheme()
{
    DEBUG_WRAP_FUNCTION();
    QSettings settings;
    int themeValue = settings.value("theme", static_cast<int>(Theme::System)).toInt();
    m_currentTheme = static_cast<Theme>(themeValue);
    
    // Apply theme immediately
    updateTheme();
}

void MainWindow::setupMenuBar()
{
    DEBUG_WRAP_FUNCTION();
    QMenuBar *menuBar = this->menuBar();
    
    // File menu
    QMenu *fileMenu = menuBar->addMenu(tr("&File"));
    QAction *exportAction = fileMenu->addAction(tr("&Export Data"));
    fileMenu->addSeparator();
    QAction *exitAction = fileMenu->addAction(tr("E&xit"));
    
    // Tools menu
    QMenu *toolsMenu = menuBar->addMenu(tr("&Tools"));
    QAction *firewallAction = toolsMenu->addAction(tr("&Firewall Rules"));
    QAction *alertsAction = toolsMenu->addAction(tr("&Alerts"));
    QAction *settingsAction = toolsMenu->addAction(tr("&Settings"));
    
    // View menu
    QMenu *viewMenu = menuBar->addMenu(tr("&View"));
    QAction *refreshAction = viewMenu->addAction(tr("&Refresh"));
    QAction *resetAction = viewMenu->addAction(tr("&Reset View"));
    
    // Theme submenu
    QMenu *themeMenu = viewMenu->addMenu(tr("&Theme"));
    m_themeActionGroup = new QActionGroup(this);
    
    m_systemThemeAction = new QAction(tr("&System"), this);
    m_systemThemeAction->setCheckable(true);
    m_systemThemeAction->setData(static_cast<int>(Theme::System));
    m_themeActionGroup->addAction(m_systemThemeAction);
    themeMenu->addAction(m_systemThemeAction);
    
    m_lightThemeAction = new QAction(tr("&Light"), this);
    m_lightThemeAction->setCheckable(true);
    m_lightThemeAction->setData(static_cast<int>(Theme::Light));
    m_themeActionGroup->addAction(m_lightThemeAction);
    themeMenu->addAction(m_lightThemeAction);
    
    m_darkThemeAction = new QAction(tr("&Dark"), this);
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setData(static_cast<int>(Theme::Dark));
    m_themeActionGroup->addAction(m_darkThemeAction);
    themeMenu->addAction(m_darkThemeAction);
    
    // Help menu
    QMenu *helpMenu = menuBar->addMenu(tr("&Help"));
    QAction *aboutAction = helpMenu->addAction(tr("&About"));
    
    // Connect actions
    connect(exportAction, &QAction::triggered, this, &MainWindow::exportData);
    connect(exitAction, &QAction::triggered, this, &QApplication::quit);
    connect(firewallAction, &QAction::triggered, this, &MainWindow::showFirewallRules);
    connect(alertsAction, &QAction::triggered, this, &MainWindow::showAlerts);
    connect(refreshAction, &QAction::triggered, this, &MainWindow::refreshData);
}

QString MainWindow::formatBytes(quint64 bytes) const
{
    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        unitIndex++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIndex]);
}

void MainWindow::refreshData()
{
    if (m_networkMonitor) {
        m_networkMonitor->updateNetworkStats();
    }
    
    // Refresh interface list
    updateInterfaceList();
    
    // Update charts
    if (m_dashboardWidget) {
        m_dashboardWidget->updateBandwidthData(0, 0); // Refresh with current data
    }
    
    // Update tables
    updateApplicationList();
    updateConnectionList();
}

void MainWindow::updateApplicationList()
{
    if (!m_networkMonitor) return;
    
    auto stats = m_networkMonitor->getStatsByApplication();
    if (m_appModel) {
        m_appModel->updateData(stats);
    }
}

void MainWindow::updateConnectionList()
{
    if (!m_networkMonitor) return;
    
    auto connections = m_networkMonitor->getActiveConnections();
    // Update connection table with new data
    // Implementation depends on connection table model
}
