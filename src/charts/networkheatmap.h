#ifndef NETWORKHEATMAP_H
#define NETWORKHEATMAP_H

#include <QWidget>
#include <QDateTime>
#include <QMap>
#include <QVector>
#include <QColor>

class NetworkHeatmap : public QWidget
{
    Q_OBJECT
    
public:
    explicit NetworkHeatmap(QWidget *parent = nullptr);
    ~NetworkHeatmap();

    // Set time range for the heatmap (default: last 24 hours)
    void setTimeRange(const QDateTime &start, const QDateTime &end);
    
    // Set time resolution in seconds (default: 1 hour)
    void setTimeResolution(int seconds);
    
    // Data management
    void addDataPoint(const QDateTime &timestamp, quint64 bytes, bool isUpload = false);
    void clear();
    void setTimeRange(const QDateTime &start, const QDateTime &end);
    void setTimeResolution(int seconds);
    
    // Data export
    bool exportToCsv(const QString &filename) const;
    bool exportToImage(const QString &filename, const char *format = "PNG") const;
    
    // Display options
    void setShowUploadData(bool show);
    void setShowDownloadData(bool show);
    void setShowCombinedData(bool show);
    void setGradientStops(const QGradientStops &stops);
    void setXAxisLabel(const QString &label);
    void setYAxisLabel(const QString &label);
    void setLegendVisible(bool visible);
    void setAxisLabelsVisible(bool visible);
    
    // Get current data range
    QDateTime startTime() const { return m_startTime; }
    QDateTime endTime() const { return m_endTime; }
    
    // Get current display options
    bool isUploadDataVisible() const { return m_showUpload; }
    bool isDownloadDataVisible() const { return m_showDownload; }
    bool isCombinedDataVisible() const { return m_showCombined; }
    
protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    
private:
    struct TimeSlot {
        quint64 downloadBytes;
        quint64 uploadBytes;
        TimeSlot() : downloadBytes(0), uploadBytes(0) {}
    };
    
    void drawHeatmap(QPainter &painter);
    void drawXAxis(QPainter &painter);
    void drawYAxis(QPainter &painter);
    void drawLegend(QPainter &painter);
    void updateBuffers();
    QColor getColorForValue(quint64 value) const;
    
    QDateTime m_startTime;
    QDateTime m_endTime;
    int m_timeResolution; // in seconds
    
    // Data storage
    struct TimeRange {
        QDateTime start;
        QDateTime end;
        QMap<int, QVector<TimeSlot>> data; // day of week -> vector of time slots
    };
    
    QList<TimeRange> m_timeRanges; // For storing historical data
    QMap<int, QVector<TimeSlot>> m_data; // Current time range data
    
    // Display options
    bool m_showUpload;
    bool m_showDownload;
    bool m_showCombined;
    
    // Display properties
    bool m_showLegend;
    bool m_showAxisLabels;
    QString m_xAxisLabel;
    QString m_yAxisLabel;
    
    // Color gradient
    QLinearGradient m_gradient;
    quint64 m_maxValue;
    
    // Cached values for performance
    QPixmap m_heatmapBuffer;
    bool m_bufferDirty;
    
    // Hover information
    QPoint m_hoverPos;
    bool m_hovering;
    
    // Layout metrics
    QRect m_plotArea;
    int m_cellSize;
    int m_marginLeft;
    int m_marginRight;
    int m_marginTop;
    int m_marginBottom;
    
    // Day labels
    static const QStringList s_dayNames;
    static const QStringList s_hourLabels;
};

#endif // NETWORKHEATMAP_H
