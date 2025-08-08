#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>
#include <QChartView>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTabWidget>
#include <QGroupBox>
#include <QTimer>
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
#include "networkmonitor.h"
#include "firewallmanager.h"
#include "alertmanager.h"
#include "intrusiondetectionmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
class QChartView;
class QLineSeries;
class QValueAxis;
class QDateTimeAxis;
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Traffic Monitor
    void onRefreshClicked();
    void onFilterChanged(const QString &filter);
    void onSearchTextChanged(const QString &text);
    void onStartStopClicked();
    void onUpdateIntervalChanged(const QString &interval);
    void onAutoStartChanged(bool enabled);
    void onThemeChanged(const QString &theme);
    
    // Network monitoring
    void onConnectionEstablished(const NetworkMonitor::ConnectionInfo &connection);
    void onStatsUpdated(quint64 download, quint64 upload);
    
    // Traffic summary updates
    void updateTrafficSummary();
    void updateDownloadSummary(qint64 total, qint64 rate);
    void updateUploadSummary(qint64 total, qint64 rate);
    void updateTotalTraffic(qint64 total);
    
    // Chart updates
    void updateTrafficChart();
    void addDataPoint(qint64 timestamp, qint64 download, qint64 upload);
    
    // Table updates
    void updateApplicationsTable();
    void updateConnectionsTable();
    
    // System tray
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void showMainWindow();
    void hideMainWindow();
    void quitApplication();
    
    // Menu actions
    void onExitAction();
    void onAboutAction();
    
    // IP2Location download progress slots
    void onIP2LocationDownloadStarted();
    void onIP2LocationDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onIP2LocationDownloadFinished(bool success);
    void onIP2LocationDatabaseReady();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;
    void setupUI();
    void setupConnections();
    void setupSystemTray();
    void setupCharts();
    void setupTables();
    void loadSettings();
    void saveSettings();
    void setMonitoring(bool enabled);
    QString formatBytes(qint64 bytes);
    QString formatRate(qint64 bytesPerSecond);
    
    // IP2Location methods
    void initializeIP2Location();
    void showIP2LocationStatus();
    void updateIP2LocationStatus();
    void showNotification(const QString &title, const QString &message);

    // UI Elements
    QTabWidget *m_mainTabWidget;
    
    // Header
    QPushButton *m_startStopButton;
    
    // Traffic Summary
    QLabel *m_downloadTotalLabel;
    QLabel *m_downloadRateLabel;
    QLabel *m_uploadTotalLabel;
    QLabel *m_uploadRateLabel;
    QLabel *m_totalValueLabel;
    
    // Traffic Monitor Tab
    QComboBox *m_filterCombo;
    QLineEdit *m_searchBox;
    QPushButton *m_refreshButton;
    QChartView *m_trafficChartView;
    QTableWidget *m_applicationsTable;
    
    // Connections Tab
    QTableWidget *m_connectionsTable;
    
    // Settings Tab
    QComboBox *m_updateIntervalCombo;
    QCheckBox *m_autoStartCheckBox;
    QComboBox *m_themeCombo;
    
    // Network monitoring
    NetworkMonitor *m_networkMonitor;
    FirewallManager *m_firewallManager;
    AlertManager *m_alertManager;
    IntrusionDetectionManager *m_intrusionDetectionManager;
    
    // Data storage
    QList<NetworkMonitor::ConnectionInfo> m_connections;
    QMap<QString, qint64> m_appTraffic;
    
    // Traffic summary data
    qint64 m_totalDownload;
    qint64 m_totalUpload;
    qint64 m_currentDownloadRate;
    qint64 m_currentUploadRate;
    
    // Chart data
    QLineSeries *m_downloadSeries;
    QLineSeries *m_uploadSeries;
    QValueAxis *m_valueAxis;
    QDateTimeAxis *m_timeAxis;
    QList<QPair<qint64, QPair<qint64, qint64>>> m_chartData;
    
    // Timers
    QTimer *m_updateTimer;
    QTimer *m_chartUpdateTimer;
    QTimer *m_connectionsUpdateTimer;
    
    // System tray
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_showAction;
    QAction *m_hideAction;
    QAction *m_quitAction;
    
    // Settings
    QSettings *m_settings;
    bool m_isMonitoring;
    bool m_minimizeToTray;
    
    // UI state
    QString m_currentFilter;
    QString m_searchText;
    
    // IP2Location progress dialog
    QProgressDialog *m_downloadProgressDialog;
    QLabel *m_ip2LocationStatusLabel;
};

#endif // MAINWINDOW_H
