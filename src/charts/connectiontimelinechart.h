#ifndef CONNECTIONTIMELINECHART_H
#define CONNECTIONTIMELINECHART_H

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QValueAxis>

#include <QDateTime>
#include <QDebug>

/**
 * @brief The ConnectionTimelineChart class provides a widget for displaying connection history over time.
 * 
 * This chart displays connection count, bandwidth usage, and other network metrics over time.
 * The x-axis represents time, and the y-axis represents the metric values.
 */
class ConnectionTimelineChart : public QChartView
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a ConnectionTimelineChart with the given parent.
     * @param parent The parent widget.
     */
    explicit ConnectionTimelineChart(QWidget *parent = nullptr);

    ~ConnectionTimelineChart() override;

public slots:
    /**
     * @brief Adds a new data point to the connection count timeline.
     * @param timestamp The timestamp for the data point.
     * @param connectionCount The number of active connections.
     */
    void addConnectionCountPoint(const QDateTime &timestamp, int connectionCount);

    /**
     * @brief Adds a new data point to the bandwidth timeline.
     * @param timestamp The timestamp for the data point.
     * @param downloadSpeed Download speed in bytes per second.
     * @param uploadSpeed Upload speed in bytes per second.
     */
    void addBandwidthPoint(const QDateTime &timestamp, quint64 downloadSpeed, quint64 uploadSpeed);

    /**
     * @brief Clears all data from the chart.
     */
    void clear();

    /**
     * @brief Sets the chart title.
     * @param title The title to display.
     */
    void setTitle(const QString &title);

    /**
     * @brief Sets the time range to display (in minutes).
     * @param minutes Number of minutes to show in the timeline.
     */
    void setTimeRange(int minutes);

    /**
     * @brief Sets which metrics to display.
     * @param showConnections True to show connection count.
     * @param showBandwidth True to show bandwidth usage.
     */
    void setVisibleMetrics(bool showConnections, bool showBandwidth);

signals:
    /**
     * @brief Emitted when a data point is clicked.
     * @param timestamp The timestamp of the clicked point.
     * @param value The value at that timestamp.
     */
    void pointClicked(const QDateTime &timestamp, double value);

private slots:
    void onPointClicked(const QPointF &point);

private:
    void setupChart();
    void setupAxes();
    void setupSeries();
    void updateTimeRange();
    QString formatSpeed(quint64 bytesPerSecond) const;

    QChart *m_chart;
    QLineSeries *m_connectionSeries;
    QLineSeries *m_downloadSeries;
    QLineSeries *m_uploadSeries;
    QDateTimeAxis *m_axisX;
    QValueAxis *m_axisY;
    
    int m_timeRangeMinutes;
    bool m_showConnections;
    bool m_showBandwidth;
    
    static const int DEFAULT_TIME_RANGE = 60; // 60 minutes
    static const int MAX_TIME_RANGE = 1440;   // 24 hours
    static const int MIN_TIME_RANGE = 5;      // 5 minutes
};

#endif // CONNECTIONTIMELINECHART_H 