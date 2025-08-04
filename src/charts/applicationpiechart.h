#ifndef APPLICATIONPIECHART_H
#define APPLICATIONPIECHART_H

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QPieSeries>
#include <QDebug>

QT_CHARTS_USE_NAMESPACE

/**
 * @brief The ApplicationPieChart class provides a widget for displaying application network usage as a pie chart.
 * 
 * This chart displays application network usage with each application represented as a pie slice.
 * The size of each slice represents the proportion of total network usage.
 */
class ApplicationPieChart : public QChartView
{
    Q_OBJECT

public:
    /**
     * @brief Constructs an ApplicationPieChart with the given parent.
     * @param parent The parent widget.
     */
    explicit ApplicationPieChart(QWidget *parent = nullptr);

    ~ApplicationPieChart() override;

public slots:
    /**
     * @brief Updates the chart with new application data.
     * @param appData Map of application names to their network usage (bytes).
     */
    void updateData(const QMap<QString, quint64> &appData);

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
     * @brief Sets whether to show percentages on slices.
     * @param show True to show percentages, false to hide them.
     */
    void setShowPercentages(bool show);

    /**
     * @brief Sets the maximum number of applications to show (others will be grouped as "Others").
     * @param maxApps Maximum number of applications to display individually.
     */
    void setMaxApplications(int maxApps);

signals:
    /**
     * @brief Emitted when a pie slice is clicked.
     * @param appName The name of the application that was clicked.
     * @param usage The network usage for that application.
     */
    void sliceClicked(const QString &appName, quint64 usage);

private slots:
    void onSliceClicked(QPieSlice *slice);

private:
    void setupChart();
    void setupSeries();
    void updateColors();
    QString formatBytes(quint64 bytes) const;
    QString formatPercentage(double percentage) const;

    QChart *m_chart;
    QPieSeries *m_pieSeries;
    
    bool m_showPercentages;
    int m_maxApplications;
    static const int DEFAULT_MAX_APPLICATIONS = 8;
    static const int MAX_ALLOWED_APPLICATIONS = 20;
    
    QMap<QString, quint64> m_currentData;
};

#endif // APPLICATIONPIECHART_H 