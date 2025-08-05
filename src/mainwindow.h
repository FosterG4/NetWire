#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QWidget>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QTimer>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QActionGroup>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QTableView>
#include <QTableWidget>
#include <QComboBox>
#include <QLabel>
#include "networkmonitor.h"
#include "firewallrulesdialog.h"
#include "alertmanager.h"
#include "alertsdialog.h"
#include "dashboard/dashboardwidget.h"
#include "charts/networkheatmap.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class ApplicationTableModel : public QAbstractTableModel
{
    Q_OBJECT
    
public:
    explicit ApplicationTableModel(QObject *parent = nullptr);
    
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    
    void updateData(const QMap<QString, NetworkMonitor::NetworkStats> &stats);
    
private:
    QString formatBytes(quint64 bytes) const {
        static const char *suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
        int i = 0;
        double size = bytes;
        while (size >= 1024 && i < 6) {
            size /= 1024;
            i++;
        }
        return QString("%1 %2").arg(size, 0, 'f', i > 0 ? 2 : 0).arg(suffixes[i]);
    }
    
    QString formatSpeed(double bytesPerSecond) const {
        static const char *units[] = {"B/s", "KB/s", "MB/s", "GB/s", "TB/s"};
        int i = 0;
        double speed = bytesPerSecond;
        while (speed >= 1024 && i < 4) {
            speed /= 1024;
            i++;
        }
        return QString("%1 %2").arg(speed, 0, 'f', i > 0 ? 2 : 0).arg(units[i]);
    }
    
    struct AppData {
        QString name;
        QIcon icon;
        quint64 download;
        quint64 upload;
        QString downloadSpeed;
        QString uploadSpeed;
    };
    
    QVector<AppData> m_apps;
    QMap<QString, quint64> m_previousDownload;
    QMap<QString, quint64> m_previousUpload;
    QDateTime m_lastUpdate;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    // Theme management
    enum class Theme { System, Light, Dark };
    
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void updateNetworkStats();
    void onStartStopClicked();
    void onInterfaceSelected(int index);
    void showContextMenu(const QPoint &pos);
    void blockApplication();
    void showApplicationDetails();
    void updateInterfaceList();
    void setMonitoring(bool enabled);
    void exportData();
    void showFirewallRules();
    void showAlerts();
    void onAlertReceived(const AlertManager::Alert &alert);
    void switchTheme(bool save, Theme theme);
    void updateTheme();
    void loadTheme();
    
    // System tray slots
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void showTrayMenu();
    void minimizeToTray();
    void restoreFromTray();
    void quitApplication();
    
    // Test functions
    void testAlertScenarios();

private:
    void setupUI();
    void setupMenuBar();
    void setupToolbar();
    void setupConnections();
    void setupSystemTray();
    void setupCharts();
    void updateTrafficGraph();
    void updateApplicationList();
    void updateConnectionList();
    void updateDashboard();
    void updateStatusBar();
    QString formatBytes(quint64 bytes) const;
    QString formatSpeed(double bytesPerSecond) const;
    
    Ui::MainWindow *ui;
    NetworkMonitor *m_networkMonitor;
    
    // Application data model
    ApplicationTableModel *m_appsModel;
    
    // UI components
    QLabel *m_statusLabel;
    
    // Theme management
    Theme m_currentTheme;
    QActionGroup *m_themeActionGroup;
    QAction *m_systemThemeAction;
    QAction *m_lightThemeAction;
    QAction *m_darkThemeAction;
    
    // Charts and series
    QChart *m_trafficChart;
    QLineSeries *m_downloadSeries;
    QLineSeries *m_uploadSeries;
    QValueAxis *m_axisX;
    QValueAxis *m_axisY;
    
    // Data models
    ApplicationTableModel *m_appModel;
    QSortFilterProxyModel *m_sortFilterModel;
    
    // Data storage
    QVector<qreal> m_downloadData;
    QVector<qreal> m_uploadData;
    QDateTime m_lastUpdateTime;
    quint64 m_totalDownloaded;
    quint64 m_totalUploaded;
    
    // Firewall
    FirewallRulesDialog *m_firewallDialog;
    
    // Alerts
    AlertManager *m_alertManager;
    AlertsDialog *m_alertsDialog;
    
    // Dashboard
    DashboardWidget *m_dashboardWidget;
    
    // Network activity heatmap
    NetworkHeatmap *m_networkHeatmap;
    
    // System tray
    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_showAction;
    QAction *m_hideAction;
    QAction *m_quitAction;
    
    // UI Components
    QAction *m_startStopAction;
    QComboBox *m_interfaceCombo;
    QTableView *m_appsTable;
    QTableWidget *m_connectionsTable;
    QLabel *m_downloadLabel;
    QLabel *m_uploadLabel;
    QLabel *m_totalLabel;
    
    // Settings
    QString m_currentInterface;
    bool m_isMonitoring;
};

#endif // MAINWINDOW_H 
