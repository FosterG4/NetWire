#include "networkheatmap.h"
#include <QPainter>
#include <QDateTime>
#include <QMouseEvent>
#include <QToolTip>
#include <QLinearGradient>
#include <QDebug>

// Static member initialization
const QStringList NetworkHeatmap::s_dayNames = {
    tr("Mon"), tr("Tue"), tr("Wed"), tr("Thu"), 
    tr("Fri"), tr("Sat"), tr("Sun")
};

const QStringList NetworkHeatmap::s_hourLabels = {
    "00:00", "06:00", "12:00", "18:00", "23:59"
};

NetworkHeatmap::NetworkHeatmap(QWidget *parent)
    : QWidget(parent)
    , m_timeResolution(3600) // 1 hour
    , m_showUpload(true)
    , m_showDownload(true)
    , m_showCombined(true)
    , m_showLegend(true)
    , m_showAxisLabels(true)
    , m_maxValue(1) // Avoid division by zero
    , m_bufferDirty(true)
    , m_hovering(false)
    , m_cellSize(20)
    , m_marginLeft(60)
    , m_marginRight(20)
    , m_marginTop(20)
    , m_marginBottom(40)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(400, 300);
    
    // Set default time range (last 7 days)
    m_endTime = QDateTime::currentDateTime();
    m_startTime = m_endTime.addDays(-6);
    
    // Initialize gradient
    m_gradient.setColorAt(0.0, Qt::blue);
    m_gradient.setColorAt(0.25, Qt::cyan);
    m_gradient.setColorAt(0.5, Qt::green);
    m_gradient.setColorAt(0.75, Qt::yellow);
    m_gradient.setColorAt(1.0, Qt::red);
    
    // Initialize data structure
    clear();
}

NetworkHeatmap::~NetworkHeatmap()
{
}

void NetworkHeatmap::setTimeRange(const QDateTime &start, const QDateTime &end)
{
    if (start != m_startTime || end != m_endTime) {
        m_startTime = start;
        m_endTime = end;
        m_bufferDirty = true;
        update();
    }
}

void NetworkHeatmap::setTimeResolution(int seconds)
{
    if (seconds > 0 && seconds != m_timeResolution) {
        m_timeResolution = seconds;
        m_bufferDirty = true;
        update();
    }
}

void NetworkHeatmap::addDataPoint(const QDateTime &timestamp, quint64 bytes, bool isUpload)
{
    if (timestamp < m_startTime || timestamp > m_endTime) {
        return; // Outside our time range
    }
    
    int dayOfWeek = timestamp.date().dayOfWeek() - 1; // 0=Monday, 6=Sunday
    if (dayOfWeek < 0 || dayOfWeek > 6) return;
    
    // Calculate time slot
    int secondsSinceMidnight = timestamp.time().msecsSinceStartOfDay() / 1000;
    int slot = secondsSinceMidnight / m_timeResolution;
    
    // Ensure we have enough slots
    int maxSlots = 86400 / m_timeResolution; // 24 hours in slots
    while (m_data[dayOfWeek].size() <= slot) {
        m_data[dayOfWeek].resize(slot + 1);
    }
    
    // Update the appropriate counter
    if (isUpload) {
        m_data[dayOfWeek][slot].uploadBytes += bytes;
    } else {
        m_data[dayOfWeek][slot].downloadBytes += bytes;
    }
    
    // Update max value for scaling
    quint64 total = m_data[dayOfWeek][slot].downloadBytes + m_data[dayOfWeek][slot].uploadBytes;
    if (total > m_maxValue) {
        m_maxValue = total;
    }
    
    m_bufferDirty = true;
    update();
}

void NetworkHeatmap::clear()
{
    m_data.clear();
    m_timeRanges.clear();
    for (int i = 0; i < 7; ++i) {
        m_data[i] = QVector<TimeSlot>(24, TimeSlot());
    }
    m_maxValue = 1;
    m_bufferDirty = true;
    update();
}

void NetworkHeatmap::setShowUploadData(bool show)
{
    if (m_showUpload != show) {
        m_showUpload = show;
        m_bufferDirty = true;
        update();
    }
}

void NetworkHeatmap::setShowDownloadData(bool show)
{
    if (m_showDownload != show) {
        m_showDownload = show;
        m_bufferDirty = true;
        update();
    }
}

void NetworkHeatmap::setShowCombinedData(bool show)
{
    if (m_showCombined != show) {
        m_showCombined = show;
        m_bufferDirty = true;
        update();
    }
}

bool NetworkHeatmap::exportToCsv(const QString &filename) const
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    
    // Write header
    out << "Day,Time,Download (B),Upload (B),Total (B)\n";
    
    // Write data
    for (int day = 0; day < 7; ++day) {
        for (int hour = 0; hour < 24; ++hour) {
            if (day < m_data.size() && hour < m_data[day].size()) {
                const TimeSlot &slot = m_data[day][hour];
                quint64 total = slot.downloadBytes + slot.uploadBytes;
                
                out << s_dayNames[day] << ","
                    << QString("%1:00").arg(hour, 2, 10, QLatin1Char('0')) << ","
                    << slot.downloadBytes << ","
                    << slot.uploadBytes << ","
                    << total << "\n";
            }
        }
    }
    
    file.close();
    return true;
}

bool NetworkHeatmap::exportToImage(const QString &filename, const char *format) const
{
    // Create a pixmap to render the heatmap
    QPixmap pixmap(size());
    pixmap.fill(Qt::transparent);
    
    // Create a painter for the pixmap
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw the heatmap onto the pixmap
    QStyleOption opt;
    opt.initFrom(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);
    
    // Draw the heatmap content
    if (m_bufferDirty) {
        // Create a temporary buffer if needed
        NetworkHeatmap *temp = const_cast<NetworkHeatmap*>(this);
        temp->updateBuffers();
    }
    
    // Draw the heatmap buffer
    painter.drawPixmap(0, 0, m_heatmapBuffer);
    
    // Save the pixmap to a file
    return pixmap.save(filename, format);
}

void NetworkHeatmap::setGradientStops(const QGradientStops &stops)
{
    m_gradient.setStops(stops);
    m_bufferDirty = true;
    update();
}

void NetworkHeatmap::setXAxisLabel(const QString &label)
{
    if (m_xAxisLabel != label) {
        m_xAxisLabel = label;
        update();
    }
}

void NetworkHeatmap::setYAxisLabel(const QString &label)
{
    if (m_yAxisLabel != label) {
        m_yAxisLabel = label;
        update();
    }
}

void NetworkHeatmap::setLegendVisible(bool visible)
{
    if (m_showLegend != visible) {
        m_showLegend = visible;
        update();
    }
}

void NetworkHeatmap::setAxisLabelsVisible(bool visible)
{
    if (m_showAxisLabels != visible) {
        m_showAxisLabels = visible;
        update();
    }
}

void NetworkHeatmap::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Update layout
    updateLayout();
    
    // Draw background
    painter.fillRect(rect(), palette().window());
    
    // Draw title
    QFont titleFont = font();
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(rect().adjusted(0, 5, 0, -5), Qt::AlignTop | Qt::AlignHCenter, 
                    tr("Network Activity Heatmap"));
    
    // Draw time range
    QFont rangeFont = font();
    rangeFont.setPointSizeF(rangeFont.pointSizeF() * 0.9);
    painter.setFont(rangeFont);
    
    QString rangeText = tr("Showing data from %1 to %2")
                       .arg(m_startTime.toString("yyyy-MM-dd"))
                       .arg(m_endTime.toString("yyyy-MM-dd"));
    painter.drawText(rect().adjusted(0, 25, 0, -25), Qt::AlignTop | Qt::AlignHCenter, rangeText);
    
    // Draw heatmap
    if (m_bufferDirty) {
        updateBuffers();
    }
    
    // Draw the heatmap with a drop shadow effect
    QPainterPath path;
    path.addRoundedRect(m_plotArea.adjusted(2, 2, 2, 2), 4, 4);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 50));
    painter.drawPath(path);
    
    painter.drawPixmap(m_plotArea, m_heatmapBuffer);
    
    // Draw axes
    drawXAxis(painter);
    drawYAxis(painter);
    
    // Draw legend
    if (m_showLegend) {
        drawLegend(painter);
    }
    
    // Draw hover effect and tooltip
    if (m_hovering) {
        int day = (m_hoverPos.x() - m_plotArea.left()) / m_cellSize;
        int hour = (m_hoverPos.y() - m_plotArea.top()) / m_cellSize;
        
        if (day >= 0 && day < 7 && hour >= 0 && hour < 24) {
            // Highlight the hovered cell
            QRect cellRect(
                m_plotArea.left() + day * m_cellSize,
                m_plotArea.top() + hour * m_cellSize,
                m_cellSize,
                m_cellSize
            );
            
            painter.setPen(QPen(Qt::white, 2));
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(cellRect.adjusted(1, 1, -1, -1));
            
            // Draw a summary tooltip
            if (day < m_data.size() && hour < m_data[day].size()) {
                const TimeSlot &slot = m_data[day][hour];
                quint64 total = slot.downloadBytes + slot.uploadBytes;
                
                // Calculate position for tooltip (try to keep it inside the widget)
                QRect tooltipRect = cellRect.adjusted(0, -80, 100, 0);
                if (tooltipRect.right() > width() - 10) {
                    tooltipRect.moveRight(cellRect.left() - 5);
                }
                if (tooltipRect.top() < 10) {
                    tooltipRect.moveTop(cellRect.bottom() + 5);
                }
                
                // Draw tooltip background
                painter.setPen(QPen(palette().color(QPalette::ToolTipText), 1));
                painter.setBrush(palette().color(QPalette::ToolTipBase));
                painter.drawRoundedRect(tooltipRect, 4, 4);
                
                // Draw tooltip text
                painter.setPen(palette().color(QPalette::ToolTipText));
                QFont tooltipFont = font();
                tooltipFont.setBold(true);
                painter.setFont(tooltipFont);
                
                int y = tooltipRect.top() + 15;
                painter.drawText(10, y, tr("Time: %1, %2:00").arg(s_dayNames[day]).arg(hour, 2, 10, QLatin1Char('0')));
                
                tooltipFont.setBold(false);
                painter.setFont(tooltipFont);
                
                if (m_showDownload) {
                    y += 18;
                    painter.drawText(10, y, tr("Download: %1").arg(formatBytes(slot.downloadBytes)));
                }
                
                if (m_showUpload) {
                    y += 16;
                    painter.drawText(10, y, tr("Upload: %1").arg(formatBytes(slot.uploadBytes)));
                }
                
                if (m_showCombined) {
                    y += 16;
                    painter.drawText(10, y, tr("Total: %1").arg(formatBytes(total)));
                }
            }
        }
    }
}

void NetworkHeatmap::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event);
    m_bufferDirty = true;
}

void NetworkHeatmap::mouseMoveEvent(QMouseEvent *event)
{
    m_hovering = m_plotArea.contains(event->pos());
    m_hoverPos = event->pos();
    
    if (m_hovering) {
        int day = (m_hoverPos.x() - m_plotArea.left()) / m_cellSize;
        int hour = (m_hoverPos.y() - m_plotArea.top()) / m_cellSize;
        
        if (day >= 0 && day < 7 && hour >= 0 && hour < 24) {
            quint64 download = m_data[day][hour].downloadBytes;
            quint64 upload = m_data[day][hour].uploadBytes;
            
            QString tooltip = QString("%1, %2:00\n")
                .arg(s_dayNames[day])
                .arg(hour, 2, 10, QLatin1Char('0'));
                
            tooltip += QString("Download: %1\n").arg(formatBytes(download));
            tooltip += QString("Upload: %1\n").arg(formatBytes(upload));
            tooltip += QString("Total: %1").arg(formatBytes(download + upload));
            
            QToolTip::showText(event->globalPos(), tooltip, this);
        }
    } else {
        QToolTip::hideText();
    }
    
    update();
}

void NetworkHeatmap::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_hovering = false;
    QToolTip::hideText();
    update();
}

void NetworkHeatmap::updateLayout()
{
    // Calculate cell size based on available space
    int width = this->width();
    int height = this->height();
    
    // Adjust margins if showing legend
    int legendWidth = m_showLegend ? 150 : 0;
    
    // Calculate plot area
    m_plotArea = QRect(
        m_marginLeft,
        m_marginTop,
        width - m_marginLeft - m_marginRight - legendWidth,
        height - m_marginTop - m_marginBottom
    );
    
    // Calculate cell size (7 days x 24 hours)
    m_cellSize = qMin(
        m_plotArea.width() / 7,
        m_plotArea.height() / 24
    );
    
    // Adjust plot area to be an exact multiple of cell size
    m_plotArea.setWidth(m_cellSize * 7);
    m_plotArea.setHeight(m_cellSize * 24);
    
    // Center the plot area
    m_plotArea.moveLeft((width - m_plotArea.width() - legendWidth) / 2);
    
    m_bufferDirty = true;
}

void NetworkHeatmap::updateBuffers()
{
    if (!m_bufferDirty) return;
    
    // Create a pixmap for the heatmap
    m_heatmapBuffer = QPixmap(m_plotArea.size());
    m_heatmapBuffer.fill(Qt::transparent);
    
    QPainter painter(&m_heatmapBuffer);
    painter.setRenderHint(QPainter::Antialiasing, false);
    
    // Draw each cell
    for (int day = 0; day < 7; ++day) {
        for (int hour = 0; hour < 24; ++hour) {
            // Calculate cell rectangle
            QRect cellRect(
                day * m_cellSize,
                hour * m_cellSize,
                m_cellSize,
                m_cellSize
            );
            
            // Get data for this cell
            TimeSlot slot = m_data[day][hour];
            quint64 total = slot.downloadBytes + slot.uploadBytes;
            
            // Calculate color based on value
            qreal value = qMin(1.0, qLn(1 + total) / qLn(1 + m_maxValue));
            QColor color = m_gradient.stops().value(0).second; // Default to first color
            
            // Find the appropriate color from the gradient
            const auto stops = m_gradient.stops();
            for (int i = 1; i < stops.size(); ++i) {
                if (value <= stops[i].first) {
                    qreal t = (value - stops[i-1].first) / (stops[i].first - stops[i-1].first);
                    color = QColor::fromRgbF(
                        stops[i-1].second.redF() * (1 - t) + stops[i].second.redF() * t,
                        stops[i-1].second.greenF() * (1 - t) + stops[i].second.greenF() * t,
                        stops[i-1].second.blueF() * (1 - t) + stops[i].second.blueF() * t
                    );
                    break;
                }
            }
            
            // Draw the cell
            painter.fillRect(cellRect, color);
        }
    }
    
    // Draw grid lines
    painter.setPen(QPen(palette().dark(), 0.5, Qt::DotLine));
    
    // Vertical lines (days)
    for (int day = 0; day <= 7; ++day) {
        int x = day * m_cellSize;
        painter.drawLine(x, 0, x, m_plotArea.height());
    }
    
    // Horizontal lines (hours)
    for (int hour = 0; hour <= 24; ++hour) {
        int y = hour * m_cellSize;
        painter.drawLine(0, y, m_plotArea.width(), y);
    }
    
    m_bufferDirty = false;
}

void NetworkHeatmap::drawXAxis(QPainter &painter)
{
    // Save painter state
    painter.save();
    
    // Set up pen and font
    QPen pen = painter.pen();
    pen.setColor(palette().text().color());
    painter.setPen(pen);
    
    QFont font = painter.font();
    font.setPointSizeF(font.pointSizeF() * 0.8);
    painter.setFont(font);
    
    // Draw day labels
    for (int day = 0; day < 7; ++day) {
        int x = m_plotArea.left() + day * m_cellSize + m_cellSize / 2;
        int y = m_plotArea.bottom() + 20;
        
        painter.drawText(
            QRect(x - 30, y, 60, 20),
            Qt::AlignCenter,
            s_dayNames[day]
        );
    }
    
    // Draw X axis label
    if (!m_xAxisLabel.isEmpty() && m_showAxisLabels) {
        painter.drawText(
            QRect(m_plotArea.left(), m_plotArea.bottom() + 40, m_plotArea.width(), 20),
            Qt::AlignCenter,
            m_xAxisLabel
        );
    }
    
    // Restore painter state
    painter.restore();
}

void NetworkHeatmap::drawYAxis(QPainter &painter)
{
    // Save painter state
    painter.save();
    
    // Set up pen and font
    QPen pen = painter.pen();
    pen.setColor(palette().text().color());
    painter.setPen(pen);
    
    QFont font = painter.font();
    font.setPointSizeF(font.pointSizeF() * 0.8);
    painter.setFont(font);
    
    // Draw hour labels
    for (int i = 0; i < s_hourLabels.size(); ++i) {
        int hour = i * 6; // 0, 6, 12, 18, 24
        if (hour >= 24) hour = 23; // Ensure we don't go out of bounds
        
        int y = m_plotArea.top() + hour * m_cellSize + m_cellSize / 2;
        
        painter.drawText(
            QRect(0, y - 10, m_marginLeft - 5, 20),
            Qt::AlignRight | Qt::AlignVCenter,
            s_hourLabels[i]
        );
    }
    
    // Draw Y axis label
    if (!m_yAxisLabel.isEmpty() && m_showAxisLabels) {
        painter.save();
        painter.translate(10, m_plotArea.center().y());
        painter.rotate(-90);
        painter.drawText(QRect(-100, 0, 200, 20), Qt::AlignCenter, m_yAxisLabel);
        painter.restore();
    }
    
    // Restore painter state
    painter.restore();
}

void NetworkHeatmap::drawLegend(QPainter &painter)
{
    // Save painter state
    painter.save();
    
    // Set up pen and font
    QPen pen = painter.pen();
    pen.setColor(palette().text().color());
    painter.setPen(pen);
    
    QFont font = painter.font();
    font.setPointSizeF(font.pointSizeF() * 0.8);
    painter.setFont(font);
    
    // Calculate legend position and size
    int legendWidth = 100;
    int legendHeight = 200;
    int legendX = m_plotArea.right() + 20;
    int legendY = m_plotArea.top();
    
    // Draw legend title
    painter.drawText(
        QRect(legendX, legendY - 20, legendWidth, 20),
        Qt::AlignCenter,
        tr("Activity")
    );
    
    // Draw gradient
    QLinearGradient gradient(0, 0, 0, legendHeight);
    gradient.setStops(m_gradient.stops());
    
    painter.setBrush(gradient);
    painter.setPen(Qt::NoPen);
    painter.drawRect(legendX, legendY, 20, legendHeight);
    
    // Draw scale
    QVector<qreal> scaleValues = {0.0, 0.25, 0.5, 0.75, 1.0};
    
    for (qreal value : scaleValues) {
        int y = legendY + (1.0 - value) * legendHeight;
        
        // Draw tick mark
        painter.setPen(pen);
        painter.drawLine(legendX + 20, y, legendX + 25, y);
        
        // Draw value
        quint64 scaledValue = qExp(value * qLn(1 + m_maxValue)) - 1;
        
        painter.drawText(
            QRect(legendX + 30, y - 10, legendWidth - 30, 20),
            Qt::AlignLeft | Qt::AlignVCenter,
            formatBytes(scaledValue)
        );
    }
    
    // Restore painter state
    painter.restore();
}

QString NetworkHeatmap::formatBytes(quint64 bytes) const
{
    const char *suffixes[] = {"B", "KB", "MB", "GB", "TB"};
    int i = 0;
    double size = bytes;
    
    while (size >= 1024 && i < 4) {
        size /= 1024;
        i++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', i > 0 ? 1 : 0).arg(suffixes[i]);
}
