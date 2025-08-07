#include "networkheatmap.h"
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QVBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QPainter>

NetworkHeatmap::NetworkHeatmap(QWidget *parent)
    : QWidget(parent)
    , m_showDownload(true)
    , m_showUpload(true)
    , m_showCombined(false)
{
    m_chart = new QChart();
    m_chartView = new QChartView(m_chart);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    
    m_downloadSeries = new QScatterSeries();
    m_downloadSeries->setName("Download");
    m_downloadSeries->setMarkerSize(8);
    
    m_uploadSeries = new QScatterSeries();
    m_uploadSeries->setName("Upload");
    m_uploadSeries->setMarkerSize(8);
    
    m_chart->addSeries(m_downloadSeries);
    m_chart->addSeries(m_uploadSeries);
    
    auto xAxis = new QDateTimeAxis();
    xAxis->setFormat("HH:mm:ss");
    m_chart->addAxis(xAxis, Qt::AlignBottom);
    m_downloadSeries->attachAxis(xAxis);
    m_uploadSeries->attachAxis(xAxis);
    
    auto yAxis = new QValueAxis();
    m_chart->addAxis(yAxis, Qt::AlignLeft);
    m_downloadSeries->attachAxis(yAxis);
    m_uploadSeries->attachAxis(yAxis);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_chartView);
    setLayout(layout);
}

void NetworkHeatmap::addDataPoint(const QDateTime &time, quint64 value, bool isUpload)
{
    if (isUpload && m_showUpload) {
        m_uploadSeries->append(time.toMSecsSinceEpoch(), value);
    } else if (!isUpload && m_showDownload) {
        m_downloadSeries->append(time.toMSecsSinceEpoch(), value);
    }
}

void NetworkHeatmap::setShowDownloadData(bool show)
{
        m_showDownload = show;
    m_downloadSeries->setVisible(show);
    }

void NetworkHeatmap::setShowUploadData(bool show)
{
    m_showUpload = show;
    m_uploadSeries->setVisible(show);
}

void NetworkHeatmap::setShowCombinedData(bool show)
{
        m_showCombined = show;
}

void NetworkHeatmap::setXAxisLabel(const QString &label)
{
    m_chart->axes(Qt::Horizontal).first()->setTitleText(label);
}

void NetworkHeatmap::setYAxisLabel(const QString &label)
{
    m_chart->axes(Qt::Vertical).first()->setTitleText(label);
}

void NetworkHeatmap::setLegendVisible(bool visible)
{
    m_chart->legend()->setVisible(visible);
}

void NetworkHeatmap::setTimeRange(const QDateTime &start, const QDateTime &end)
{
    auto xAxis = qobject_cast<QDateTimeAxis*>(m_chart->axes(Qt::Horizontal).first());
    if (xAxis) {
        xAxis->setRange(start, end);
    }
}

bool NetworkHeatmap::exportToCsv(const QString &filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << "Time,Download,Upload\n";
    
    // Export data points
    // Implementation depends on data storage
    
    return true;
}

bool NetworkHeatmap::exportToImage(const QString &filename, const char *format) const
{
    return m_chartView->grab().save(filename, format);
}
