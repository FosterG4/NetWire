#include "connectiontimelinechart.h"
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>
#include <QDateTime>
#include <QTimer>

ConnectionTimelineChart::ConnectionTimelineChart(QWidget *parent)
    : QChartView(parent)
    , m_chart(new QChart)
    , m_connectionSeries(new QLineSeries)
    , m_downloadSeries(new QLineSeries)
    , m_uploadSeries(new QLineSeries)
    , m_axisX(new QDateTimeAxis)
    , m_axisY(new QValueAxis)
    , m_timeRangeMinutes(DEFAULT_TIME_RANGE)
    , m_showConnections(true)
    , m_showBandwidth(true)
{
    setupChart();
    setupAxes();
    setupSeries();
}

ConnectionTimelineChart::~ConnectionTimelineChart()
{
    delete m_chart;
}

void ConnectionTimelineChart::setupChart()
{
    m_chart->setTitle(tr("Connection Timeline"));
    m_chart->setTitleFont(QFont("Segoe UI", 12, QFont::Bold));
    m_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_chart->setTheme(QChart::ChartThemeLight);
    
    // Set chart margins
    m_chart->setMargins(QMargins(10, 10, 10, 10));
    
    setChart(m_chart);
    setRenderHint(QPainter::Antialiasing);
}

void ConnectionTimelineChart::setupAxes()
{
    // Setup X-axis (time)
    m_axisX->setTitleText(tr("Time"));
    m_axisX->setFormat("HH:mm:ss");
    m_axisX->setTickCount(6);
    
    // Setup Y-axis (values)
    m_axisY->setTitleText(tr("Value"));
    m_axisY->setLabelFormat("%.0f");
    m_axisY->setTickCount(6);
    
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
}

void ConnectionTimelineChart::setupSeries()
{
    // Setup connection count series
    m_connectionSeries->setName(tr("Connections"));
    m_connectionSeries->setColor(QColor(52, 152, 219)); // Blue
    m_connectionSeries->setVisible(m_showConnections);
    
    // Setup download series
    m_downloadSeries->setName(tr("Download"));
    m_downloadSeries->setColor(QColor(46, 204, 113)); // Green
    m_downloadSeries->setVisible(m_showBandwidth);
    
    // Setup upload series
    m_uploadSeries->setName(tr("Upload"));
    m_uploadSeries->setColor(QColor(231, 76, 60)); // Red
    m_uploadSeries->setVisible(m_showBandwidth);
    
    // Add series to chart
    if (m_showConnections) {
        m_chart->addSeries(m_connectionSeries);
        m_connectionSeries->attachAxis(m_axisX);
        m_connectionSeries->attachAxis(m_axisY);
    }
    
    if (m_showBandwidth) {
        m_chart->addSeries(m_downloadSeries);
        m_chart->addSeries(m_uploadSeries);
        m_downloadSeries->attachAxis(m_axisX);
        m_downloadSeries->attachAxis(m_axisY);
        m_uploadSeries->attachAxis(m_axisX);
        m_uploadSeries->attachAxis(m_axisY);
    }
    
    // Connect point clicked signal
    connect(m_connectionSeries, &QLineSeries::clicked, this, &ConnectionTimelineChart::onPointClicked);
    connect(m_downloadSeries, &QLineSeries::clicked, this, &ConnectionTimelineChart::onPointClicked);
    connect(m_uploadSeries, &QLineSeries::clicked, this, &ConnectionTimelineChart::onPointClicked);
}

void ConnectionTimelineChart::addConnectionCountPoint(const QDateTime &timestamp, int connectionCount)
{
    if (!m_showConnections) {
        return;
    }
    
    // Add point to series
    m_connectionSeries->append(timestamp.toMSecsSinceEpoch(), connectionCount);
    
    // Remove old points outside time range
    QDateTime cutoffTime = timestamp.addSecs(-m_timeRangeMinutes * 60);
    qint64 cutoffMs = cutoffTime.toMSecsSinceEpoch();
    
    QList<QPointF> points = m_connectionSeries->points();
    for (int i = points.size() - 1; i >= 0; --i) {
        if (points[i].x() < cutoffMs) {
            m_connectionSeries->removePoints(0, i + 1);
            break;
        }
    }
    
    updateTimeRange();
}

void ConnectionTimelineChart::addBandwidthPoint(const QDateTime &timestamp, quint64 downloadSpeed, quint64 uploadSpeed)
{
    if (!m_showBandwidth) {
        return;
    }
    
    // Add points to series
    m_downloadSeries->append(timestamp.toMSecsSinceEpoch(), downloadSpeed);
    m_uploadSeries->append(timestamp.toMSecsSinceEpoch(), uploadSpeed);
    
    // Remove old points outside time range
    QDateTime cutoffTime = timestamp.addSecs(-m_timeRangeMinutes * 60);
    qint64 cutoffMs = cutoffTime.toMSecsSinceEpoch();
    
    QList<QPointF> downloadPoints = m_downloadSeries->points();
    QList<QPointF> uploadPoints = m_uploadSeries->points();
    
    for (int i = downloadPoints.size() - 1; i >= 0; --i) {
        if (downloadPoints[i].x() < cutoffMs) {
            m_downloadSeries->removePoints(0, i + 1);
            break;
        }
    }
    
    for (int i = uploadPoints.size() - 1; i >= 0; --i) {
        if (uploadPoints[i].x() < cutoffMs) {
            m_uploadSeries->removePoints(0, i + 1);
            break;
        }
    }
    
    updateTimeRange();
}

void ConnectionTimelineChart::clear()
{
    m_connectionSeries->clear();
    m_downloadSeries->clear();
    m_uploadSeries->clear();
    
    // Reset axes
    QDateTime now = QDateTime::currentDateTime();
    m_axisX->setRange(now.addSecs(-m_timeRangeMinutes * 60), now);
    m_axisY->setRange(0, 100);
}

void ConnectionTimelineChart::setTitle(const QString &title)
{
    m_chart->setTitle(title);
}

void ConnectionTimelineChart::setTimeRange(int minutes)
{
    if (minutes >= MIN_TIME_RANGE && minutes <= MAX_TIME_RANGE) {
        m_timeRangeMinutes = minutes;
        updateTimeRange();
    }
}

void ConnectionTimelineChart::setVisibleMetrics(bool showConnections, bool showBandwidth)
{
    m_showConnections = showConnections;
    m_showBandwidth = showBandwidth;
    
    // Update series visibility
    m_connectionSeries->setVisible(showConnections);
    m_downloadSeries->setVisible(showBandwidth);
    m_uploadSeries->setVisible(showBandwidth);
    
    // Re-add series to chart if needed
    m_chart->removeAllSeries();
    
    if (showConnections) {
        m_chart->addSeries(m_connectionSeries);
        m_connectionSeries->attachAxis(m_axisX);
        m_connectionSeries->attachAxis(m_axisY);
    }
    
    if (showBandwidth) {
        m_chart->addSeries(m_downloadSeries);
        m_chart->addSeries(m_uploadSeries);
        m_downloadSeries->attachAxis(m_axisX);
        m_downloadSeries->attachAxis(m_axisY);
        m_uploadSeries->attachAxis(m_axisX);
        m_uploadSeries->attachAxis(m_axisY);
    }
}

void ConnectionTimelineChart::onPointClicked(const QPointF &point)
{
    QDateTime timestamp = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(point.x()));
    emit pointClicked(timestamp, point.y());
}

void ConnectionTimelineChart::updateTimeRange()
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime startTime = now.addSecs(-m_timeRangeMinutes * 60);
    
    m_axisX->setRange(startTime, now);
    
    // Update Y-axis range based on visible data
    double maxValue = 0;
    
    if (m_showConnections && !m_connectionSeries->points().isEmpty()) {
        for (const QPointF &point : m_connectionSeries->points()) {
            maxValue = qMax(maxValue, point.y());
        }
    }
    
    if (m_showBandwidth) {
        for (const QPointF &point : m_downloadSeries->points()) {
            maxValue = qMax(maxValue, point.y());
        }
        for (const QPointF &point : m_uploadSeries->points()) {
            maxValue = qMax(maxValue, point.y());
        }
    }
    
    // Set Y-axis range with some padding
    if (maxValue > 0) {
        m_axisY->setRange(0, maxValue * 1.1);
    } else {
        m_axisY->setRange(0, 100);
    }
}

QString ConnectionTimelineChart::formatSpeed(quint64 bytesPerSecond) const
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