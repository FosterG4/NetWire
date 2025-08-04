#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QValueAxis>
#include <QTimer>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QMenu>
#include <QActionGroup>
#include <QSettings>
#include <QSystemTrayIcon>
#include "networkmonitor.h"
#include "firewallrulesdialog.h"
#include "alertmanager.h"
#include "alertsdialog.h"
#include "dashboard/dashboardwidget.h"

QT_CHARTS_USE_NAMESPACE
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
    void showFirewallRules();
    void showAlerts();
    void onAlertReceived(const AlertManager::Alert &alert);
    void switchTheme(bool checked);
    void updateTheme();
    
    // System tray slots
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void showTrayMenu();
    void minimizeToTray();
    void restoreFromTray();
    void quitApplication();

private:
    void setupUI();
    void setupConnections();
    void setupCharts();
    void setupSystemTray();
    void updateTrafficGraph();
    void updateApplicationList();
    void updateConnectionList();
    void updateStatusBar();
    QString formatBytes(quint64 bytes) const;
    QString formatSpeed(double bytesPerSecond) const;
    
    Ui::MainWindow *ui;
    NetworkMonitor *m_networkMonitor;
    
    // Theme management
    enum class Theme { System, Light, Dark };
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
