#ifndef NETWORKCHARTS_H
#define NETWORKCHARTS_H

#include <QWidget>
#include <QLabel>
#include <QMap>

class NetworkCharts : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkCharts(QWidget *parent = nullptr);

public slots:
    /**
     * @brief Updates the connection statistics display
     * @param stats Map of statistic names to values
     */
    void updateConnectionStats(const QMap<QString, quint64> &stats);

private:
    void setupLayout();
    
    // Labels for displaying statistics
    QLabel *m_totalConnectionsLabel;
    QLabel *m_tcpConnectionsLabel;
    QLabel *m_udpConnectionsLabel;
    QLabel *m_bytesReceivedLabel;
    QLabel *m_bytesSentLabel;
    
    // Format a byte count into a human-readable string
    QString formatByteSize(quint64 bytes) const;
};

#endif // NETWORKCHARTS_H