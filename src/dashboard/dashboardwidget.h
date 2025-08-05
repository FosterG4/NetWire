#ifndef DASHBOARDWIDGET_H
#define DASHBOARDWIDGET_H

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QGroupBox>

class BandwidthChart;
class NetworkCharts;
class ApplicationPieChart;
class ConnectionTimelineChart;

class DashboardWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardWidget(QWidget *parent = nullptr);

public slots:
    void updateBandwidthData(quint64 download, quint64 upload);
    void updateConnectionCount(int count);
    void updateSystemResources(double cpu, double memory);

private:
    void setupLayout();
    void setupStatisticsWidgets();
    void setupCharts();

    // Statistics widgets
    QLabel *m_downloadSpeedLabel;
    QLabel *m_uploadSpeedLabel;
    QLabel *m_connectionCountLabel;
    QLabel *m_cpuUsageLabel;
    QLabel *m_memoryUsageLabel;

    // Charts
    BandwidthChart *m_bandwidthChart;
    NetworkCharts *m_networkCharts;
    ApplicationPieChart *m_applicationPieChart;
    ConnectionTimelineChart *m_connectionTimelineChart;

    // Layout containers
    QGroupBox *m_statisticsGroup;
    QGroupBox *m_chartsGroup;
    QGroupBox *m_pieChartGroup;
    QGroupBox *m_timelineGroup;
};

#endif // DASHBOARDWIDGET_H 