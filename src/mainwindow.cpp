#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QApplication>
#include <QIcon>
#include <QCloseEvent>
#include <QCheckBox>
#include <QProgressDialog>
#include <QStatusBar>

#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QChartView>
#include <QtCharts/QChartGlobal>

#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QApplication>
#include <QStyle>
#include <QScreen>
#include <QDateTime>
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QStatusBar>
#include <QMenuBar>
#include <QCloseEvent>
#include <QDebug>
#include <QApplication>
#include <QIcon>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QHostInfo>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QProcess>
#include <QThread>
#include <QFuture>
#include <QtConcurrent>
#include <QRandomGenerator>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_networkMonitor(new NetworkMonitor(this))
    , m_firewallManager(FirewallManager::instance())
    , m_alertManager(AlertManager::instance())
    , m_intrusionDetectionManager(IntrusionDetectionManager::instance())
    , m_totalDownload(0)
    , m_totalUpload(0)
    , m_currentDownloadRate(0)
    , m_currentUploadRate(0)
    , m_isMonitoring(false)
    , m_minimizeToTray(false)
    , m_settings(new QSettings("NetWire", "NetWire", this))
{
    setupUI();
    setupConnections();
    setupSystemTray();
    setupCharts();
    setupTables();
    loadSettings();
    
    // Initialize IP2Location
    initializeIP2Location();
    
    // Start monitoring by default
    setMonitoring(true);
    
    // Set up timers
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(1000); // Update every second
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateTrafficSummary);
    m_updateTimer->start();
    
    m_chartUpdateTimer = new QTimer(this);
    m_chartUpdateTimer->setInterval(5000); // Update chart every 5 seconds
    connect(m_chartUpdateTimer, &QTimer::timeout, this, &MainWindow::updateTrafficChart);
    m_chartUpdateTimer->start();
    
    // Add timer for updating connections table (less frequent to reduce lag)
    m_connectionsUpdateTimer = new QTimer(this);
    m_connectionsUpdateTimer->setInterval(5000); // Update connections every 5 seconds (reduced frequency)
    connect(m_connectionsUpdateTimer, &QTimer::timeout, this, &MainWindow::updateConnectionsTable);
    m_connectionsUpdateTimer->start();
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::setupUI()
{
    ui->setupUi(this);
    
    // Store UI element pointers
    m_mainTabWidget = ui->mainTabWidget;
    m_startStopButton = ui->startStopButton;
    
    // Traffic Summary
    m_downloadTotalLabel = ui->downloadTotalLabel;
    m_downloadRateLabel = ui->downloadRateLabel;
    m_uploadTotalLabel = ui->uploadTotalLabel;
    m_uploadRateLabel = ui->uploadRateLabel;
    m_totalValueLabel = ui->totalValueLabel;
    
    // Traffic Monitor Tab
    m_filterCombo = ui->filterCombo;
    m_searchBox = ui->searchBox;
    m_refreshButton = ui->refreshButton;
    m_trafficChartView = ui->trafficChartView;
    m_applicationsTable = ui->applicationsTable;
    
    // Connections Tab
    m_connectionsTable = ui->connectionsTable;
    
    // Settings Tab
    m_updateIntervalCombo = ui->updateIntervalCombo;
    m_autoStartCheckBox = ui->autoStartCheckBox;
    m_themeCombo = ui->themeCombo;
    
    // Set window properties
    setWindowTitle("NetWire - Network Monitor");
    resize(800, 600);
    
    // Center window on screen
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        QRect screenGeometry = screen->geometry();
        int x = (screenGeometry.width() - width()) / 2;
        int y = (screenGeometry.height() - height()) / 2;
        move(x, y);
    }
}

void MainWindow::setupConnections()
{
    // Traffic Monitor connections
    connect(m_refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(m_filterCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged), 
            this, &MainWindow::onFilterChanged);
    connect(m_searchBox, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
    connect(m_startStopButton, &QPushButton::clicked, this, &MainWindow::onStartStopClicked);
    
    // Settings connections
    connect(m_updateIntervalCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &MainWindow::onUpdateIntervalChanged);
    connect(m_autoStartCheckBox, &QCheckBox::toggled, this, &MainWindow::onAutoStartChanged);
    connect(m_themeCombo, QOverload<const QString &>::of(&QComboBox::currentTextChanged),
            this, &MainWindow::onThemeChanged);
    
    // Network monitoring connections
    connect(m_networkMonitor, &NetworkMonitor::connectionEstablished, 
            this, &MainWindow::onConnectionEstablished);
    connect(m_networkMonitor, &NetworkMonitor::statsUpdated, 
            this, &MainWindow::onStatsUpdated);
    
    // Menu connections
    connect(ui->actionExit, &QAction::triggered, this, &MainWindow::onExitAction);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onAboutAction);
}

void MainWindow::setupSystemTray()
{
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/icons/app.ico"));
    m_trayIcon->setToolTip("NetWire - Network Monitor");
    
    m_trayMenu = new QMenu(this);
    m_showAction = new QAction("Show", this);
    m_hideAction = new QAction("Hide", this);
    m_quitAction = new QAction("Quit", this);
    
    m_trayMenu->addAction(m_showAction);
    m_trayMenu->addAction(m_hideAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_quitAction);
    
    m_trayIcon->setContextMenu(m_trayMenu);
    
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &MainWindow::onTrayIconActivated);
    connect(m_showAction, &QAction::triggered, this, &MainWindow::showMainWindow);
    connect(m_hideAction, &QAction::triggered, this, &MainWindow::hideMainWindow);
    connect(m_quitAction, &QAction::triggered, this, &MainWindow::quitApplication);
    
    m_trayIcon->show();
}

void MainWindow::setupCharts()
{
    // Create chart
    auto chart = new QChart();
    chart->setTitle("Real-Time Network Traffic");
    chart->setAnimationOptions(QChart::SeriesAnimations);
    
    // Create series
    m_downloadSeries = new QLineSeries();
    m_downloadSeries->setName("Download");
    m_downloadSeries->setColor(QColor("#27ae60"));
    
    m_uploadSeries = new QLineSeries();
    m_uploadSeries->setName("Upload");
    m_uploadSeries->setColor(QColor("#e74c3c"));
    
    chart->addSeries(m_downloadSeries);
    chart->addSeries(m_uploadSeries);
    
    // Create axes
    m_timeAxis = new QDateTimeAxis();
    m_timeAxis->setTickCount(10);
    m_timeAxis->setFormat("HH:mm:ss");
    m_timeAxis->setTitleText("Time");
    
    m_valueAxis = new QValueAxis();
    m_valueAxis->setLabelFormat("%.1f KB/s");
    m_valueAxis->setTitleText("Traffic Rate");
    m_valueAxis->setRange(0, 1000);
    
    chart->addAxis(m_timeAxis, Qt::AlignBottom);
    chart->addAxis(m_valueAxis, Qt::AlignLeft);
    
    m_downloadSeries->attachAxis(m_timeAxis);
    m_downloadSeries->attachAxis(m_valueAxis);
    m_uploadSeries->attachAxis(m_timeAxis);
    m_uploadSeries->attachAxis(m_valueAxis);
    
    m_trafficChartView->setChart(chart);
    m_trafficChartView->setRenderHint(QPainter::Antialiasing);
}

void MainWindow::setupTables()
{
    // Applications table
    m_applicationsTable->setColumnCount(4);
    m_applicationsTable->setHorizontalHeaderLabels({"Application", "Download", "Upload", "Total"});
    m_applicationsTable->horizontalHeader()->setStretchLastSection(true);
    m_applicationsTable->setAlternatingRowColors(true);
    m_applicationsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_applicationsTable->setSortingEnabled(true);
    
    // Make row numbers visible and properly sized for applications table too
    m_applicationsTable->verticalHeader()->setVisible(true);
    m_applicationsTable->verticalHeader()->setDefaultSectionSize(25);
    m_applicationsTable->verticalHeader()->setMinimumSectionSize(20);
    m_applicationsTable->verticalHeader()->setMaximumSectionSize(30);
    m_applicationsTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    
    // Connections table with enhanced columns
    m_connectionsTable->setColumnCount(8);
    m_connectionsTable->setHorizontalHeaderLabels({
        "Application", "Local Address", "Remote Address", "Remote Host", 
        "Protocol", "Traffic Type", "Status", "Country"
    });
    m_connectionsTable->horizontalHeader()->setStretchLastSection(true);
    m_connectionsTable->setAlternatingRowColors(true);
    m_connectionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_connectionsTable->setSortingEnabled(true);
    
    // Make row numbers visible and properly sized
    m_connectionsTable->verticalHeader()->setVisible(true);
    m_connectionsTable->verticalHeader()->setDefaultSectionSize(25);
    m_connectionsTable->verticalHeader()->setMinimumSectionSize(20);
    m_connectionsTable->verticalHeader()->setMaximumSectionSize(30);
    m_connectionsTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    
    // Set column widths for better display
    m_connectionsTable->setColumnWidth(0, 120); // Application
    m_connectionsTable->setColumnWidth(1, 120); // Local Address
    m_connectionsTable->setColumnWidth(2, 120); // Remote Address
    m_connectionsTable->setColumnWidth(3, 150); // Remote Host
    m_connectionsTable->setColumnWidth(4, 80);  // Protocol
    m_connectionsTable->setColumnWidth(5, 100); // Traffic Type
    m_connectionsTable->setColumnWidth(6, 100); // Status
    m_connectionsTable->setColumnWidth(7, 80);  // Country
}

void MainWindow::loadSettings()
{
    m_settings->beginGroup("MainWindow");
    restoreGeometry(m_settings->value("geometry").toByteArray());
    restoreState(m_settings->value("windowState").toByteArray());
    m_minimizeToTray = m_settings->value("minimizeToTray", false).toBool();
    m_settings->endGroup();
    
    m_settings->beginGroup("Monitoring");
    m_isMonitoring = m_settings->value("isMonitoring", true).toBool();
    m_autoStartCheckBox->setChecked(m_settings->value("autoStart", true).toBool());
    m_settings->endGroup();
    
    m_settings->beginGroup("Display");
    QString theme = m_settings->value("theme", "Light").toString();
    int themeIndex = m_themeCombo->findText(theme);
    if (themeIndex >= 0) {
        m_themeCombo->setCurrentIndex(themeIndex);
    }
    m_settings->endGroup();
}

void MainWindow::saveSettings()
{
    m_settings->beginGroup("MainWindow");
    m_settings->setValue("geometry", saveGeometry());
    m_settings->setValue("windowState", saveState());
    m_settings->setValue("minimizeToTray", m_minimizeToTray);
    m_settings->endGroup();
    
    m_settings->beginGroup("Monitoring");
    m_settings->setValue("isMonitoring", m_isMonitoring);
    m_settings->setValue("autoStart", m_autoStartCheckBox->isChecked());
    m_settings->endGroup();
    
    m_settings->beginGroup("Display");
    m_settings->setValue("theme", m_themeCombo->currentText());
    m_settings->endGroup();
    
    m_settings->sync();
}

void MainWindow::setMonitoring(bool enabled)
{
    m_isMonitoring = enabled;
    
    if (enabled) {
        QStringList interfaces = m_networkMonitor->getAvailableInterfaces();
        if (!interfaces.isEmpty()) {
            m_networkMonitor->startCapture(interfaces.first());
        }
        m_startStopButton->setText("Stop Monitoring");
        ui->statusbar->showMessage("Monitoring active");
    } else {
        m_networkMonitor->stopCapture();
        m_startStopButton->setText("Start Monitoring");
        ui->statusbar->showMessage("Monitoring stopped");
    }
}

QString MainWindow::formatBytes(qint64 bytes)
{
    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = bytes;
    
    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        unitIndex++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', 1).arg(units[unitIndex]);
}

QString MainWindow::formatRate(qint64 bytesPerSecond)
{
    return formatBytes(bytesPerSecond) + "/s";
}

// Traffic Monitor slots
void MainWindow::onRefreshClicked()
{
    updateApplicationsTable();
    updateConnectionsTable();
    ui->statusbar->showMessage("Data refreshed", 2000);
}

void MainWindow::onFilterChanged(const QString &filter)
{
    m_currentFilter = filter;
    updateApplicationsTable();
    ui->statusbar->showMessage(QString("Filter changed to: %1").arg(filter), 2000);
}

void MainWindow::onSearchTextChanged(const QString &text)
{
    m_searchText = text;
    updateApplicationsTable();
}

void MainWindow::onStartStopClicked()
{
    setMonitoring(!m_isMonitoring);
}

void MainWindow::onUpdateIntervalChanged(const QString &interval)
{
    int intervalMs = 1000; // Default 1 second
    
    if (interval.contains("5 Seconds")) {
        intervalMs = 5000;
    } else if (interval.contains("10 Seconds")) {
        intervalMs = 10000;
    }
    
    m_updateTimer->setInterval(intervalMs);
    m_chartUpdateTimer->setInterval(intervalMs * 5); // Chart updates 5x slower
    
    ui->statusbar->showMessage(QString("Update interval changed to: %1").arg(interval), 2000);
}

void MainWindow::onAutoStartChanged(bool enabled)
{
    m_settings->beginGroup("Monitoring");
    m_settings->setValue("autoStart", enabled);
    m_settings->endGroup();
    
    ui->statusbar->showMessage(enabled ? "Auto-start enabled" : "Auto-start disabled", 2000);
}

void MainWindow::onThemeChanged(const QString &theme)
{
    // Simple theme switching - could be expanded
    if (theme == "Dark") {
        // Apply dark theme stylesheet
        setStyleSheet("QMainWindow { background-color: #2c3e50; color: #ecf0f1; }");
    } else {
        // Apply light theme stylesheet
        setStyleSheet("QMainWindow { background-color: #f5f5f5; color: #333333; }");
    }
    
    m_settings->beginGroup("Display");
    m_settings->setValue("theme", theme);
    m_settings->endGroup();
    
    ui->statusbar->showMessage(QString("Theme changed to: %1").arg(theme), 2000);
}

// Network monitoring slots
void MainWindow::onConnectionEstablished(const NetworkMonitor::ConnectionInfo &connection)
{
    m_connections.append(connection);
    updateConnectionsTable();
}

void MainWindow::onStatsUpdated(quint64 download, quint64 upload)
{
    m_currentDownloadRate = download;
    m_currentUploadRate = upload;
    m_totalDownload += download;
    m_totalUpload += upload;
    
    updateDownloadSummary(m_totalDownload, m_currentDownloadRate);
    updateUploadSummary(m_totalUpload, m_currentUploadRate);
    updateTotalTraffic(m_totalDownload + m_totalUpload);
    
    // Add data point to chart
    addDataPoint(QDateTime::currentMSecsSinceEpoch(), download, upload);
}



// Traffic summary updates
void MainWindow::updateTrafficSummary()
{
    // This is called by timer to update summary periodically
    // Could add additional calculations here
}

void MainWindow::updateDownloadSummary(qint64 total, qint64 rate)
{
    m_downloadTotalLabel->setText(formatBytes(total));
    m_downloadRateLabel->setText(QString("↓ %1").arg(formatRate(rate)));
}

void MainWindow::updateUploadSummary(qint64 total, qint64 rate)
{
    m_uploadTotalLabel->setText(formatBytes(total));
    m_uploadRateLabel->setText(QString("↑ %1").arg(formatRate(rate)));
}

void MainWindow::updateTotalTraffic(qint64 total)
{
    m_totalValueLabel->setText(formatBytes(total));
}

// Chart updates
void MainWindow::updateTrafficChart()
{
    // This is called by timer to update chart periodically
    // Could add additional chart updates here
}

void MainWindow::addDataPoint(qint64 timestamp, qint64 download, qint64 upload)
{
    // Keep only last 100 data points
    if (m_chartData.size() > 100) {
        m_chartData.removeFirst();
    }
    
    m_chartData.append(qMakePair(timestamp, qMakePair(download, upload)));
    
    // Update chart series
    m_downloadSeries->clear();
    m_uploadSeries->clear();
    
    for (const auto &dataPoint : m_chartData) {
        qint64 timestamp = dataPoint.first;
        qint64 download = dataPoint.second.first;
        qint64 upload = dataPoint.second.second;
        
        m_downloadSeries->append(timestamp, download / 1024.0); // Convert to KB
        m_uploadSeries->append(timestamp, upload / 1024.0); // Convert to KB
    }
    
    // Update axes
    if (!m_chartData.isEmpty()) {
        qint64 minTime = m_chartData.first().first;
        qint64 maxTime = m_chartData.last().first;
        m_timeAxis->setRange(QDateTime::fromMSecsSinceEpoch(minTime), 
                            QDateTime::fromMSecsSinceEpoch(maxTime));
        
        // Find max value for Y axis
        double maxValue = 0;
        for (const auto &dataPoint : m_chartData) {
            maxValue = qMax(maxValue, qMax(dataPoint.second.first, dataPoint.second.second) / 1024.0);
        }
        m_valueAxis->setRange(0, maxValue * 1.1); // Add 10% padding
    }
}

// Table updates
void MainWindow::updateApplicationsTable()
{
    m_applicationsTable->setRowCount(0);
    
    for (auto it = m_appTraffic.begin(); it != m_appTraffic.end(); ++it) {
        const QString &appName = it.key();
        qint64 total = it.value();
        
        // Simple filtering - could be enhanced
        if (!m_searchText.isEmpty() && !appName.contains(m_searchText, Qt::CaseInsensitive)) {
            continue;
        }
        
        int row = m_applicationsTable->rowCount();
        m_applicationsTable->insertRow(row);
        
        m_applicationsTable->setItem(row, 0, new QTableWidgetItem(appName));
        m_applicationsTable->setItem(row, 1, new QTableWidgetItem(formatBytes(total * 0.6))); // Simulated download
        m_applicationsTable->setItem(row, 2, new QTableWidgetItem(formatBytes(total * 0.4))); // Simulated upload
        m_applicationsTable->setItem(row, 3, new QTableWidgetItem(formatBytes(total)));
    }
}

void MainWindow::updateConnectionsTable()
{
    // Get real-time connections from NetworkMonitor
    QList<NetworkMonitor::ConnectionInfo> activeConnections = m_networkMonitor->getActiveConnections();
    QMap<QString, QString> hostnameCache = m_networkMonitor->getHostnameCache();
    
    // Debug: Show connection count in status bar
    ui->statusbar->showMessage(QString("Active connections: %1").arg(activeConnections.size()));
    
    // Limit the number of connections to display to prevent performance issues
    const int maxConnections = 100; // Limit to 100 connections for better performance
    if (activeConnections.size() > maxConnections) {
        activeConnections = activeConnections.mid(0, maxConnections);
        ui->statusbar->showMessage(QString("Showing %1 of %2 connections (limited for performance)").arg(maxConnections).arg(m_networkMonitor->getActiveConnections().size()));
    }
    
    // Only update if the number of connections changed significantly
    static int lastConnectionCount = -1;
    if (qAbs(activeConnections.size() - lastConnectionCount) < 3 && lastConnectionCount != -1) {
        return; // Skip update if connection count hasn't changed much
    }
    lastConnectionCount = activeConnections.size();
    
    // Clear table efficiently
    m_connectionsTable->setRowCount(0);
    
    for (const auto &connection : activeConnections) {
        int row = m_connectionsTable->rowCount();
        m_connectionsTable->insertRow(row);
        
        // Application name (column 0)
        QString appName;
        if (!connection.processName.isEmpty() && !connection.processName.startsWith("PID:")) {
            appName = connection.processName;
        } else if (!connection.processPath.isEmpty()) {
            // Extract filename from full path
            appName = QFileInfo(connection.processPath).fileName();
        } else if (connection.processId > 0) {
            appName = QString("PID:%1").arg(connection.processId);
        } else {
            appName = "Unknown";
        }
        
        auto appItem = new QTableWidgetItem(appName);
        if (!connection.processIcon.isNull()) {
            appItem->setIcon(connection.processIcon);
        }
        
        // Add tooltip with full process path if available
        if (!connection.processPath.isEmpty()) {
            appItem->setToolTip(QString("Process: %1\nPID: %2\nPath: %3")
                               .arg(appName)
                               .arg(connection.processId)
                               .arg(connection.processPath));
        } else {
            appItem->setToolTip(QString("Process: %1\nPID: %2")
                               .arg(appName)
                               .arg(connection.processId));
        }
        
        m_connectionsTable->setItem(row, 0, appItem);
        
        // Local Address:Port (column 1)
        QString localAddr = QString("%1:%2").arg(connection.localAddress).arg(connection.localPort);
        m_connectionsTable->setItem(row, 1, new QTableWidgetItem(localAddr));
        
        // Remote Address:Port (column 2)
        QString remoteAddr;
        if (connection.remoteAddress == "*" || connection.remoteAddress.isEmpty()) {
            remoteAddr = "*";
        } else {
            remoteAddr = QString("%1:%2").arg(connection.remoteAddress).arg(connection.remotePort);
        }
        m_connectionsTable->setItem(row, 2, new QTableWidgetItem(remoteAddr));
        
        // Remote Hostname (column 3)
        QString hostname = connection.remoteHostname;
        if (hostname.isEmpty() && hostnameCache.contains(connection.remoteAddress)) {
            hostname = hostnameCache.value(connection.remoteAddress);
        }
        if (hostname.isEmpty() || hostname == connection.remoteAddress) {
            hostname = "-";
        }
        m_connectionsTable->setItem(row, 3, new QTableWidgetItem(hostname));
        
        // Protocol (column 4)
        QString protocol = (connection.protocol == 6) ? "TCP" : 
                          (connection.protocol == 17) ? "UDP" : 
                          QString("Proto-%1").arg(connection.protocol);
        m_connectionsTable->setItem(row, 4, new QTableWidgetItem(protocol));
        
        // Traffic Type (column 5)
        QString trafficType = connection.serviceName.isEmpty() ? 
                             m_networkMonitor->getTrafficType(connection.remotePort, connection.protocol) :
                             connection.serviceName;
        m_connectionsTable->setItem(row, 5, new QTableWidgetItem(trafficType));
        
        // Status (column 6)
        auto statusItem = new QTableWidgetItem(connection.connectionState);
        // Color-code status for better visibility
        if (connection.connectionState == "ESTABLISHED") {
            statusItem->setForeground(QColor("#27ae60")); // Green
        } else if (connection.connectionState == "LISTENING") {
            statusItem->setForeground(QColor("#3498db")); // Blue
        } else if (connection.connectionState.contains("WAIT") || connection.connectionState.contains("CLOSING")) {
            statusItem->setForeground(QColor("#f39c12")); // Orange
        }
        m_connectionsTable->setItem(row, 6, statusItem);
        
        // Country (column 7)
        QString country = m_networkMonitor->getCountryFromIP(connection.remoteAddress);
        if (country.isEmpty() || country == "Unknown") {
            if (connection.remoteAddress == "*" || connection.remoteAddress.isEmpty() ||
                connection.remoteAddress.startsWith("127.") || connection.remoteAddress.startsWith("192.168.") ||
                connection.remoteAddress.startsWith("10.") || connection.remoteAddress.startsWith("172.")) {
                country = "Local";
            } else {
                country = "-";
            }
        }
        m_connectionsTable->setItem(row, 7, new QTableWidgetItem(country));
    }
    
    // Update connection count in status bar
    ui->statusbar->showMessage(QString("Active connections: %1").arg(activeConnections.size()));
}

// System tray slots
void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible()) {
            hide();
        } else {
            show();
            raise();
            activateWindow();
        }
    }
}

void MainWindow::showMainWindow()
{
    show();
    raise();
    activateWindow();
}

void MainWindow::hideMainWindow()
{
    hide();
}

void MainWindow::quitApplication()
{
    QApplication::quit();
}

// Menu action slots
void MainWindow::onExitAction()
{
    close();
}

void MainWindow::onAboutAction()
{
    QMessageBox::about(this, "About NetWire", 
        "NetWire - Network Monitor\n\n"
        "A simple network traffic monitoring application.\n\n"
        "Version 1.0\n"
        "© 2025 NetWire");
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_minimizeToTray && m_trayIcon->isVisible()) {
        hide();
        event->ignore();
    } else {
        event->accept();
    }
}

// IP2Location methods
void MainWindow::initializeIP2Location()
{
    // Create progress dialog for database download
    m_downloadProgressDialog = new QProgressDialog("Downloading IP2Location database...", "Cancel", 0, 100, this);
    m_downloadProgressDialog->setWindowModality(Qt::WindowModal);
    m_downloadProgressDialog->setAutoClose(false);
    m_downloadProgressDialog->setAutoReset(false);
    m_downloadProgressDialog->hide();
    
    // Create status label for IP2Location
    m_ip2LocationStatusLabel = new QLabel(this);
    statusBar()->addPermanentWidget(m_ip2LocationStatusLabel);
    
    // Connect IP2Location signals
    connect(m_networkMonitor, &NetworkMonitor::databaseDownloadStarted, this, &MainWindow::onIP2LocationDownloadStarted);
    connect(m_networkMonitor, &NetworkMonitor::databaseDownloadProgress, this, &MainWindow::onIP2LocationDownloadProgress);
    connect(m_networkMonitor, &NetworkMonitor::databaseDownloadFinished, this, &MainWindow::onIP2LocationDownloadFinished);
    connect(m_networkMonitor, &NetworkMonitor::databaseReady, this, &MainWindow::onIP2LocationDatabaseReady);
    
    // Update initial status
    updateIP2LocationStatus();
    
    // Start download if needed
    if (!m_networkMonitor->isIP2LocationReady()) {
        qDebug() << "IP2Location database not ready, starting download...";
        m_networkMonitor->downloadIP2LocationDatabase();
    }
}

void MainWindow::onIP2LocationDownloadStarted()
{
    qDebug() << "IP2Location download started";
    m_downloadProgressDialog->setValue(0);
    m_downloadProgressDialog->show();
    updateIP2LocationStatus();
}

void MainWindow::onIP2LocationDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int percent = (bytesReceived * 100) / bytesTotal;
        m_downloadProgressDialog->setValue(percent);
        
        QString progressText = QString("Downloading IP2Location database... %1% (%2 / %3 MB)")
            .arg(percent)
            .arg(bytesReceived / (1024 * 1024))
            .arg(bytesTotal / (1024 * 1024));
        m_downloadProgressDialog->setLabelText(progressText);
    }
}

void MainWindow::onIP2LocationDownloadFinished(bool success)
{
    m_downloadProgressDialog->hide();
    
    if (success) {
        qDebug() << "IP2Location database download completed successfully";
        showNotification("IP2Location", "Database downloaded successfully!");
    } else {
        qDebug() << "IP2Location database download failed";
        showNotification("IP2Location", "Database download failed. Will retry later.");
    }
    
    updateIP2LocationStatus();
}

void MainWindow::onIP2LocationDatabaseReady()
{
    qDebug() << "IP2Location database is ready";
    updateIP2LocationStatus();
    showNotification("IP2Location", "Database ready for IP geolocation!");
}

void MainWindow::updateIP2LocationStatus()
{
    if (m_networkMonitor->isIP2LocationReady()) {
        m_ip2LocationStatusLabel->setText("🌍 IP2Location: Ready");
        m_ip2LocationStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        m_ip2LocationStatusLabel->setText("🌍 IP2Location: Downloading...");
        m_ip2LocationStatusLabel->setStyleSheet("color: orange; font-weight: bold;");
    }
}

void MainWindow::showIP2LocationStatus()
{
    QString status = m_networkMonitor->getIP2LocationDatabaseInfo();
    QMessageBox::information(this, "IP2Location Status", status);
}

void MainWindow::showNotification(const QString &title, const QString &message)
{
    if (m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information, 3000);
    } else {
        QMessageBox::information(this, title, message);
    }
}
