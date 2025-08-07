#ifndef NETWORKHEATMAP_H
#define NETWORKHEATMAP_H

// Prevent Windows min/max macros from interfering with Qt Charts
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <QWidget>
#include <QDateTime>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>

class NetworkHeatmap : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkHeatmap(QWidget *parent = nullptr);
    
    void addDataPoint(const QDateTime &time, quint64 value, bool isUpload);
    void setShowDownloadData(bool show);
    void setShowUploadData(bool show);
    void setShowCombinedData(bool show);
    void setXAxisLabel(const QString &label);
    void setYAxisLabel(const QString &label);
    void setLegendVisible(bool visible);
    void setTimeRange(const QDateTime &start, const QDateTime &end);
    bool exportToCsv(const QString &filename) const;
    bool exportToImage(const QString &filename, const char *format = "PNG") const;

private:
    QChartView *m_chartView;
    QChart *m_chart;
    QScatterSeries *m_downloadSeries;
    QScatterSeries *m_uploadSeries;
    bool m_showDownload;
    bool m_showUpload;
    bool m_showCombined;
};

#endif // NETWORKHEATMAP_H
