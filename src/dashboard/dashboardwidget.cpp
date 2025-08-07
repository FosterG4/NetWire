#include "dashboardwidget.h"
#include "../charts/bandwidthchart.h"
#include "../charts/applicationpiechart.h"
#include "../charts/connectiontimelinechart.h"
#include "networkcharts.h"
#include <QGridLayout>
#include <QFont>
#include <QDebug>

DashboardWidget::DashboardWidget(QWidget *parent)
    : QWidget(parent)
    , m_downloadSpeedLabel(nullptr)
    , m_uploadSpeedLabel(nullptr)
    , m_connectionCountLabel(nullptr)
    , m_cpuUsageLabel(nullptr)
    , m_memoryUsageLabel(nullptr)
    , m_bandwidthChart(nullptr)
    , m_networkCharts(nullptr)
    , m_applicationPieChart(nullptr)
    , m_connectionTimelineChart(nullptr)
    , m_statisticsGroup(nullptr)
    , m_chartsGroup(nullptr)
    , m_pieChartGroup(nullptr)
    , m_timelineGroup(nullptr)
{
    setupLayout();
    setupStatisticsWidgets();
    setupCharts();
}

void DashboardWidget::setupLayout()
{
    auto mainLayout = new QVBoxLayout(this);
    
    // Statistics section
    m_statisticsGroup = new QGroupBox("Network Statistics", this);
    auto statsLayout = new QGridLayout(m_statisticsGroup);
    mainLayout->addWidget(m_statisticsGroup);
    
    // Charts section
    m_chartsGroup = new QGroupBox("Network Charts", this);
    auto chartsLayout = new QVBoxLayout(m_chartsGroup);
    mainLayout->addWidget(m_chartsGroup);
    
    // Pie chart section
    m_pieChartGroup = new QGroupBox("Application Usage", this);
    auto pieChartLayout = new QHBoxLayout(m_pieChartGroup);
    mainLayout->addWidget(m_pieChartGroup);
    
    // Timeline section
    m_timelineGroup = new QGroupBox("Connection Timeline", this);
    auto timelineLayout = new QVBoxLayout(m_timelineGroup);
    mainLayout->addWidget(m_timelineGroup);
    
    setLayout(mainLayout);
}

void DashboardWidget::setupStatisticsWidgets()
{
    auto statsLayout = qobject_cast<QGridLayout*>(m_statisticsGroup->layout());
    
    // Download speed
    auto downloadLabel = new QLabel("Download Speed:", this);
    m_downloadSpeedLabel = new QLabel("0 B/s", this);
    m_downloadSpeedLabel->setFont(QFont("Arial", 12, QFont::Bold));
    statsLayout->addWidget(downloadLabel, 0, 0);
    statsLayout->addWidget(m_downloadSpeedLabel, 0, 1);
    
    // Upload speed
    auto uploadLabel = new QLabel("Upload Speed:", this);
    m_uploadSpeedLabel = new QLabel("0 B/s", this);
    m_uploadSpeedLabel->setFont(QFont("Arial", 12, QFont::Bold));
    statsLayout->addWidget(uploadLabel, 1, 0);
    statsLayout->addWidget(m_uploadSpeedLabel, 1, 1);
    
    // Connection count
    auto connectionLabel = new QLabel("Active Connections:", this);
    m_connectionCountLabel = new QLabel("0", this);
    m_connectionCountLabel->setFont(QFont("Arial", 12, QFont::Bold));
    statsLayout->addWidget(connectionLabel, 2, 0);
    statsLayout->addWidget(m_connectionCountLabel, 2, 1);
    
    // CPU usage
    auto cpuLabel = new QLabel("CPU Usage:", this);
    m_cpuUsageLabel = new QLabel("0%", this);
    m_cpuUsageLabel->setFont(QFont("Arial", 12, QFont::Bold));
    statsLayout->addWidget(cpuLabel, 0, 2);
    statsLayout->addWidget(m_cpuUsageLabel, 0, 3);
    
    // Memory usage
    auto memoryLabel = new QLabel("Memory Usage:", this);
    m_memoryUsageLabel = new QLabel("0%", this);
    m_memoryUsageLabel->setFont(QFont("Arial", 12, QFont::Bold));
    statsLayout->addWidget(memoryLabel, 1, 2);
    statsLayout->addWidget(m_memoryUsageLabel, 1, 3);
}

void DashboardWidget::setupCharts()
{
    qDebug() << "Setting up charts...";
    
    auto chartsLayout = qobject_cast<QVBoxLayout*>(m_chartsGroup->layout());
    
    // Create bandwidth chart
    qDebug() << "Creating BandwidthChart...";
    try {
        m_bandwidthChart = new BandwidthChart(this);
        chartsLayout->addWidget(m_bandwidthChart);
        qDebug() << "BandwidthChart created successfully";
    } catch (const std::exception& e) {
        qDebug() << "Error creating BandwidthChart:" << e.what();
        QLabel *errorLabel = new QLabel("Error creating bandwidth chart", this);
        chartsLayout->addWidget(errorLabel);
    } catch (...) {
        qDebug() << "Unknown error creating BandwidthChart";
        QLabel *errorLabel = new QLabel("Unknown error creating bandwidth chart", this);
        chartsLayout->addWidget(errorLabel);
    }
    
    // Create network charts (placeholder for now)
    qDebug() << "Creating NetworkCharts...";
    try {
        m_networkCharts = new NetworkCharts(this);
        chartsLayout->addWidget(m_networkCharts);
        qDebug() << "NetworkCharts created successfully";
    } catch (const std::exception& e) {
        qDebug() << "Error creating NetworkCharts:" << e.what();
        QLabel *errorLabel = new QLabel("Error creating network charts", this);
        chartsLayout->addWidget(errorLabel);
    } catch (...) {
        qDebug() << "Unknown error creating NetworkCharts";
        QLabel *errorLabel = new QLabel("Unknown error creating network charts", this);
        chartsLayout->addWidget(errorLabel);
    }
    
    // Create application pie chart
    qDebug() << "Creating ApplicationPieChart...";
    try {
        auto pieChartLayout = qobject_cast<QHBoxLayout*>(m_pieChartGroup->layout());
        m_applicationPieChart = new ApplicationPieChart(this);
        pieChartLayout->addWidget(m_applicationPieChart);
        qDebug() << "ApplicationPieChart created successfully";
    } catch (const std::exception& e) {
        qDebug() << "Error creating ApplicationPieChart:" << e.what();
        auto pieChartLayout = qobject_cast<QHBoxLayout*>(m_pieChartGroup->layout());
        QLabel *errorLabel = new QLabel("Error creating application pie chart", this);
        pieChartLayout->addWidget(errorLabel);
    } catch (...) {
        qDebug() << "Unknown error creating ApplicationPieChart";
        auto pieChartLayout = qobject_cast<QHBoxLayout*>(m_pieChartGroup->layout());
        QLabel *errorLabel = new QLabel("Unknown error creating application pie chart", this);
        pieChartLayout->addWidget(errorLabel);
    }
    
    // Create connection timeline chart
    qDebug() << "Creating ConnectionTimelineChart...";
    try {
        auto timelineLayout = qobject_cast<QVBoxLayout*>(m_timelineGroup->layout());
        m_connectionTimelineChart = new ConnectionTimelineChart(this);
        timelineLayout->addWidget(m_connectionTimelineChart);
        qDebug() << "ConnectionTimelineChart created successfully";
    } catch (const std::exception& e) {
        qDebug() << "Error creating ConnectionTimelineChart:" << e.what();
        auto timelineLayout = qobject_cast<QVBoxLayout*>(m_timelineGroup->layout());
        QLabel *errorLabel = new QLabel("Error creating connection timeline chart", this);
        timelineLayout->addWidget(errorLabel);
    } catch (...) {
        qDebug() << "Unknown error creating ConnectionTimelineChart";
        auto timelineLayout = qobject_cast<QVBoxLayout*>(m_timelineGroup->layout());
        QLabel *errorLabel = new QLabel("Unknown error creating connection timeline chart", this);
        timelineLayout->addWidget(errorLabel);
    }
    
    qDebug() << "All charts setup completed";
}

void DashboardWidget::updateBandwidthData(quint64 download, quint64 upload)
{
    if (m_downloadSpeedLabel) {
        m_downloadSpeedLabel->setText(QString("%1 B/s").arg(download));
    }
    if (m_uploadSpeedLabel) {
        m_uploadSpeedLabel->setText(QString("%1 B/s").arg(upload));
    }
    if (m_bandwidthChart) {
        m_bandwidthChart->addDataPoint(download, upload);
    }
}

void DashboardWidget::updateConnectionCount(int count)
{
    if (m_connectionCountLabel) {
        m_connectionCountLabel->setText(QString::number(count));
    }
}

void DashboardWidget::updateSystemResources(double cpu, double memory)
{
    if (m_cpuUsageLabel) {
        m_cpuUsageLabel->setText(QString("%1%").arg(cpu, 0, 'f', 1));
    }
    if (m_memoryUsageLabel) {
        m_memoryUsageLabel->setText(QString("%1%").arg(memory, 0, 'f', 1));
    }
} 