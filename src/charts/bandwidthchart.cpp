#include "bandwidthchart.h"
#include <QtWidgets/QApplication>
#include <QPen>
#include <QColor>
#include <QMessageBox>
#include <QEasingCurve>
#include <QPropertyAnimation>
#include <QTimer>
#include <stdexcept>
#include <QtCore/QtGlobal>

BandwidthChart::BandwidthChart(QWidget *parent)
    : QChartView(parent)
    , m_dataPointCount(0)
    , m_maxPoints(DEFAULT_MAX_POINTS)
    , m_dataUpdateAnimation(nullptr)
    , m_seriesAnimation(nullptr)
    , m_animationTimer(nullptr)
    , m_isAnimating(false)
{
    try {
        setupChart();
        setupAxes();
        setupSeries();
        setupAnimations();
    } catch (const std::exception &e) {
        qCritical() << "Failed to initialize BandwidthChart:" << e.what();
        emit errorOccurred(tr("Failed to initialize bandwidth chart: %1").arg(e.what()));
        throw;
    }
}

BandwidthChart::~BandwidthChart()
{
    // Clean up resources
    delete m_chart;  // This will also delete all child items (series, axes)
}

void BandwidthChart::addDataPoint(quint64 download, quint64 upload)
{
    try {
        if (!m_chart || !m_downloadSeries || !m_uploadSeries) {
            throw std::runtime_error("Chart not properly initialized");
        }

        // Add new data points
        m_downloadSeries->append(m_dataPointCount, static_cast<qreal>(download));
        m_uploadSeries->append(m_dataPointCount, static_cast<qreal>(upload));
        
        // Manage the number of points to display
        if (m_dataPointCount >= m_maxPoints) {
            m_downloadSeries->remove(0);
            m_uploadSeries->remove(0);
        } else {
            m_dataPointCount++;
        }
        
        // Update X axis range
        m_axisX->setRange(0, m_dataPointCount > 0 ? m_dataPointCount - 1 : 0);
        
        // Update Y axis range
        updateYAxisRange();
        
    } catch (const std::exception &e) {
        qWarning() << "Failed to add data point:" << e.what();
        emit errorOccurred(tr("Failed to update chart data: %1").arg(e.what()));
    }
}

void BandwidthChart::clear()
{
    if (m_downloadSeries) m_downloadSeries->clear();
    if (m_uploadSeries) m_uploadSeries->clear();
    m_dataPointCount = 0;
    m_axisX->setRange(0, 10);  // Reset to default range
    m_axisY->setRange(0, 1000);
}

void BandwidthChart::setMaxPoints(int maxPoints)
{
    if (maxPoints < 10) {
        qWarning() << "Requested max points too low, using minimum of 10";
        m_maxPoints = 10;
    } else if (maxPoints > MAX_ALLOWED_POINTS) {
        qWarning() << "Requested max points too high, using maximum of" << MAX_ALLOWED_POINTS;
        m_maxPoints = MAX_ALLOWED_POINTS;
    } else {
        m_maxPoints = maxPoints;
    }
    
    // If we have more points than the new maximum, trim the series
    while (m_dataPointCount > m_maxPoints) {
        m_downloadSeries->remove(0);
        m_uploadSeries->remove(0);
        m_dataPointCount--;
    }
    
    m_axisX->setRange(0, m_maxPoints - 1);
}

void BandwidthChart::setAnimating(bool animating)
{
    m_isAnimating = animating;
}

void BandwidthChart::setupAnimations()
{
    // Initialize member variables if not already done
    if (!m_dataUpdateAnimation) {
        m_dataUpdateAnimation = new QPropertyAnimation(this);
        m_dataUpdateAnimation->setDuration(ANIMATION_DURATION);
        m_dataUpdateAnimation->setEasingCurve(QEasingCurve::OutCubic);
    }
    
    if (!m_seriesAnimation) {
        m_seriesAnimation = new QPropertyAnimation(this);
        m_seriesAnimation->setDuration(ANIMATION_DURATION);
        m_seriesAnimation->setEasingCurve(QEasingCurve::OutQuad);
    }
    
    // Create animation timer if not already created
    if (!m_animationTimer) {
        m_animationTimer = new QTimer(this);
        m_animationTimer->setSingleShot(true);
        m_animationTimer->setInterval(50);
        
        // Connect the timer timeout to update the animation state
        QObject::connect(m_animationTimer, &QTimer::timeout, this, [this]() {
            setAnimating(false);
        });
    }
    
    // Initialize animation state using the setter
    setAnimating(false);
}

void BandwidthChart::updateYAxisRange()
{
    if (!m_downloadSeries || !m_uploadSeries) return;
    
    // Find maximum value in both series
    qreal maxValue = 1000;  // Default minimum range
    
    for (const QPointF &point : m_downloadSeries->points()) {
        if (point.y() > maxValue) maxValue = point.y();
    }
    for (const QPointF &point : m_uploadSeries->points()) {
        if (point.y() > maxValue) maxValue = point.y();
    }
    
    // Add 10% headroom and update axis
    maxValue *= 1.1;
    if (maxValue < 1000) maxValue = 1000;  // Minimum range
    
    m_axisY->setRange(0, maxValue);
}

void BandwidthChart::setupChart()
{
    if (!m_chart) {
        m_chart = new QChart();
        m_chart->legend()->setVisible(true);
        m_chart->setTitle(tr("Network Bandwidth"));
        setChart(m_chart);
        setRenderHint(QPainter::Antialiasing);
    }
}

void BandwidthChart::setupAxes()
{
    if (!m_chart) {
        throw std::runtime_error("Chart must be initialized before axes");
    }
    
    m_axisX = new QValueAxis();
    m_axisX->setTitleText(tr("Time"));
    m_axisX->setLabelFormat("%d");
    m_axisX->setRange(0, m_maxPoints - 1);
    
    m_axisY = new QValueAxis();
    m_axisY->setTitleText(tr("Bytes/s"));
    m_axisY->setLabelFormat("%.1f");
    m_axisY->setRange(0, 1000); // Default range, will auto-scale
    
    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
}

void BandwidthChart::setupSeries()
{
    if (!m_chart) {
        throw std::runtime_error("Chart must be initialized before series");
    }
    
    m_downloadSeries = new QLineSeries();
    m_downloadSeries->setName(tr("Download"));
    m_downloadSeries->setColor(Qt::blue);
    
    m_uploadSeries = new QLineSeries();
    m_uploadSeries->setName(tr("Upload"));
    m_uploadSeries->setColor(Qt::red);
    
    m_chart->addSeries(m_downloadSeries);
    m_chart->addSeries(m_uploadSeries);
    
    // Attach axes to series
    m_downloadSeries->attachAxis(m_axisX);
    m_downloadSeries->attachAxis(m_axisY);
    m_uploadSeries->attachAxis(m_axisX);
    m_uploadSeries->attachAxis(m_axisY);
}

 