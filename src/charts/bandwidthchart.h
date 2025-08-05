#ifndef BANDWIDTHCHART_H
#define BANDWIDTHCHART_H

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QTimer>
#include <QDebug>
#include <QPropertyAnimation>
#include <QObject>
#include <QtCore/QtGlobal>

/**
 * @brief The BandwidthChart class provides a widget for displaying network bandwidth usage over time.
 * 
 * This chart displays two line series: one for download speed and one for upload speed.
 * The x-axis represents time, and the y-axis represents data rate in bytes per second.
 */
class BandwidthChart : public QChartView
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a BandwidthChart with the given parent.
     * @param parent The parent widget.
     */
    explicit BandwidthChart(QWidget *parent = nullptr);

    ~BandwidthChart() override;

public slots:
    /**
     * @brief Adds a new data point to the chart.
     * @param download Download speed in bytes per second.
     * @param upload Upload speed in bytes per second.
     * @throws std::invalid_argument if either value is negative.
     */
    void addDataPoint(quint64 download, quint64 upload);

    /**
     * @brief Clears all data from the chart.
     */
    void clear();

    /**
     * @brief Sets the maximum number of data points to display.
     * @param maxPoints Maximum number of points to show.
     */
    void setMaxPoints(int maxPoints);

    /**
     * @brief Gets the current maximum number of data points.
     * @return The maximum number of points being displayed.
     */
    int maxPoints() const { return m_maxPoints; }
    
    /**
     * @brief Sets the animation state.
     * @param animating Whether the chart is currently animating.
     */
    void setAnimating(bool animating);

signals:
    /**
     * @brief Emitted when an error occurs during data processing.
     * @param errorMessage Description of the error that occurred.
     */
    void errorOccurred(const QString &errorMessage);

private:
    void setupChart();
    void setupAxes();
    void setupSeries();
    void setupAnimations();
    void updateYAxisRange();

    QChart *m_chart;
    QLineSeries *m_downloadSeries;
    QLineSeries *m_uploadSeries;
    QValueAxis *m_axisX;
    QValueAxis *m_axisY;
    
    int m_dataPointCount;
    int m_maxPoints;
    
    // Animation properties
    QPropertyAnimation *m_dataUpdateAnimation;
    QPropertyAnimation *m_seriesAnimation;
    QTimer *m_animationTimer;
    bool m_isAnimating;
    
    static const int DEFAULT_MAX_POINTS = 100;
    static const int MAX_ALLOWED_POINTS = 1000;
    static const int ANIMATION_DURATION = 200;
};

#endif // BANDWIDTHCHART_H