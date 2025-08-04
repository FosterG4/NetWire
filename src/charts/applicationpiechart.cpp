#include "applicationpiechart.h"
#include <QtCharts/QPieSlice>
#include <QColor>
#include <QPalette>
#include <QApplication>

ApplicationPieChart::ApplicationPieChart(QWidget *parent)
    : QChartView(parent)
    , m_chart(new QChart)
    , m_pieSeries(new QPieSeries)
    , m_showPercentages(true)
    , m_maxApplications(DEFAULT_MAX_APPLICATIONS)
{
    setupChart();
    setupSeries();
}

ApplicationPieChart::~ApplicationPieChart()
{
    delete m_chart;
}

void ApplicationPieChart::setupChart()
{
    m_chart->setTitle(tr("Application Network Usage"));
    m_chart->setTitleFont(QFont("Segoe UI", 12, QFont::Bold));
    m_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_chart->setTheme(QChart::ChartThemeLight);
    
    // Set chart margins
    m_chart->setMargins(QMargins(10, 10, 10, 10));
    
    setChart(m_chart);
    setRenderHint(QPainter::Antialiasing);
}

void ApplicationPieChart::setupSeries()
{
    m_pieSeries->setHoleSize(0.35);
    m_pieSeries->setPieSize(0.8);
    
    // Connect slice clicked signal
    connect(m_pieSeries, &QPieSeries::clicked, this, &ApplicationPieChart::onSliceClicked);
    
    m_chart->addSeries(m_pieSeries);
}

void ApplicationPieChart::updateData(const QMap<QString, quint64> &appData)
{
    m_currentData = appData;
    
    // Clear existing slices
    m_pieSeries->clear();
    
    if (appData.isEmpty()) {
        return;
    }
    
    // Calculate total usage
    quint64 totalUsage = 0;
    for (auto it = appData.constBegin(); it != appData.constEnd(); ++it) {
        totalUsage += it.value();
    }
    
    if (totalUsage == 0) {
        return;
    }
    
    // Sort applications by usage (descending)
    QList<QPair<QString, quint64>> sortedApps;
    for (auto it = appData.constBegin(); it != appData.constEnd(); ++it) {
        sortedApps.append(qMakePair(it.key(), it.value()));
    }
    
    std::sort(sortedApps.begin(), sortedApps.end(),
        [](const QPair<QString, quint64> &a, const QPair<QString, quint64> &b) {
            return a.second > b.second;
        });
    
    // Create pie slices
    quint64 othersUsage = 0;
    int appCount = 0;
    
    for (const auto &app : sortedApps) {
        if (appCount < m_maxApplications) {
            // Create individual slice
            QPieSlice *slice = new QPieSlice(app.first, app.second);
            
            double percentage = (app.second * 100.0) / totalUsage;
            QString label = QString("%1 (%2)").arg(app.first).arg(formatPercentage(percentage));
            slice->setLabel(label);
            
            if (m_showPercentages) {
                slice->setLabelVisible(true);
            } else {
                slice->setLabelVisible(false);
            }
            
            // Set color based on position
            QColor color = QColor::fromHsv((appCount * 360 / m_maxApplications) % 360, 150, 200);
            slice->setBrush(color);
            slice->setPen(QPen(color.darker(120), 1));
            
            m_pieSeries->append(slice);
            appCount++;
        } else {
            // Group remaining applications as "Others"
            othersUsage += app.second;
        }
    }
    
    // Add "Others" slice if needed
    if (othersUsage > 0) {
        QPieSlice *othersSlice = new QPieSlice(tr("Others"), othersUsage);
        
        double percentage = (othersUsage * 100.0) / totalUsage;
        QString label = QString("%1 (%2)").arg(tr("Others")).arg(formatPercentage(percentage));
        othersSlice->setLabel(label);
        
        if (m_showPercentages) {
            othersSlice->setLabelVisible(true);
        } else {
            othersSlice->setLabelVisible(false);
        }
        
        // Set gray color for "Others"
        QColor othersColor = QColor(128, 128, 128);
        othersSlice->setBrush(othersColor);
        othersSlice->setPen(QPen(othersColor.darker(120), 1));
        
        m_pieSeries->append(othersSlice);
    }
    
    // Update chart title with total usage
    QString title = QString("%1 (%2)").arg(tr("Application Network Usage")).arg(formatBytes(totalUsage));
    m_chart->setTitle(title);
}

void ApplicationPieChart::clear()
{
    m_pieSeries->clear();
    m_currentData.clear();
    m_chart->setTitle(tr("Application Network Usage"));
}

void ApplicationPieChart::setTitle(const QString &title)
{
    m_chart->setTitle(title);
}

void ApplicationPieChart::setShowPercentages(bool show)
{
    m_showPercentages = show;
    
    // Update existing slices
    for (auto slice : m_pieSeries->slices()) {
        slice->setLabelVisible(show);
    }
}

void ApplicationPieChart::setMaxApplications(int maxApps)
{
    if (maxApps > 0 && maxApps <= MAX_ALLOWED_APPLICATIONS) {
        m_maxApplications = maxApps;
        
        // Refresh chart with current data
        if (!m_currentData.isEmpty()) {
            updateData(m_currentData);
        }
    }
}

void ApplicationPieChart::onSliceClicked(QPieSlice *slice)
{
    if (slice) {
        QString appName = slice->label().split(" (").first();
        quint64 usage = static_cast<quint64>(slice->value());
        
        // Handle "Others" slice specially
        if (appName == tr("Others")) {
            appName = "Others";
        }
        
        emit sliceClicked(appName, usage);
    }
}

void ApplicationPieChart::updateColors()
{
    // Update colors for all slices
    int sliceIndex = 0;
    for (auto slice : m_pieSeries->slices()) {
        QColor color = QColor::fromHsv((sliceIndex * 360 / m_pieSeries->count()) % 360, 150, 200);
        slice->setBrush(color);
        slice->setPen(QPen(color.darker(120), 1));
        sliceIndex++;
    }
}

QString ApplicationPieChart::formatBytes(quint64 bytes) const
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;
    
    while (size >= 1024.0 && unit < 4) {
        size /= 1024.0;
        unit++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', unit > 0 ? 1 : 0).arg(units[unit]);
}

QString ApplicationPieChart::formatPercentage(double percentage) const
{
    return QString("%1%").arg(percentage, 0, 'f', 1);
} 