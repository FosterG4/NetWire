#include "networkcharts.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QGroupBox>
#include <QFont>

NetworkCharts::NetworkCharts(QWidget *parent)
    : QWidget(parent)
    , m_totalConnectionsLabel(nullptr)
    , m_tcpConnectionsLabel(nullptr)
    , m_udpConnectionsLabel(nullptr)
    , m_bytesReceivedLabel(nullptr)
    , m_bytesSentLabel(nullptr)
{
    setupLayout();
}

void NetworkCharts::setupLayout()
{
    auto mainLayout = new QVBoxLayout(this);
    
    // Create a group box for connection statistics
    auto statsGroup = new QGroupBox("Connection Statistics", this);
    auto statsLayout = new QGridLayout(statsGroup);
    
    // Create labels for statistics
    QFont labelFont("Arial", 10);
    QFont valueFont("Arial", 10, QFont::Bold);
    
    // Total connections
    auto totalConnectionsLabel = new QLabel("Total Connections:", this);
    totalConnectionsLabel->setFont(labelFont);
    m_totalConnectionsLabel = new QLabel("0", this);
    m_totalConnectionsLabel->setFont(valueFont);
    statsLayout->addWidget(totalConnectionsLabel, 0, 0);
    statsLayout->addWidget(m_totalConnectionsLabel, 0, 1);
    
    // TCP connections
    auto tcpConnectionsLabel = new QLabel("TCP Connections:", this);
    tcpConnectionsLabel->setFont(labelFont);
    m_tcpConnectionsLabel = new QLabel("0", this);
    m_tcpConnectionsLabel->setFont(valueFont);
    statsLayout->addWidget(tcpConnectionsLabel, 1, 0);
    statsLayout->addWidget(m_tcpConnectionsLabel, 1, 1);
    
    // UDP connections
    auto udpConnectionsLabel = new QLabel("UDP Connections:", this);
    udpConnectionsLabel->setFont(labelFont);
    m_udpConnectionsLabel = new QLabel("0", this);
    m_udpConnectionsLabel->setFont(valueFont);
    statsLayout->addWidget(udpConnectionsLabel, 2, 0);
    statsLayout->addWidget(m_udpConnectionsLabel, 2, 1);
    
    // Bytes received
    auto bytesReceivedLabel = new QLabel("Total Bytes Received:", this);
    bytesReceivedLabel->setFont(labelFont);
    m_bytesReceivedLabel = new QLabel("0 B", this);
    m_bytesReceivedLabel->setFont(valueFont);
    statsLayout->addWidget(bytesReceivedLabel, 0, 2);
    statsLayout->addWidget(m_bytesReceivedLabel, 0, 3);
    
    // Bytes sent
    auto bytesSentLabel = new QLabel("Total Bytes Sent:", this);
    bytesSentLabel->setFont(labelFont);
    m_bytesSentLabel = new QLabel("0 B", this);
    m_bytesSentLabel->setFont(valueFont);
    statsLayout->addWidget(bytesSentLabel, 1, 2);
    statsLayout->addWidget(m_bytesSentLabel, 1, 3);
    
    // Add spacing between columns
    statsLayout->setColumnMinimumWidth(1, 50);
    statsLayout->setColumnMinimumWidth(2, 30);
    
    // Add the group box to the main layout
    mainLayout->addWidget(statsGroup);
    
    // Add a placeholder for future charts
    auto chartsPlaceholder = new QLabel("Additional Connection Charts - Coming Soon", this);
    chartsPlaceholder->setAlignment(Qt::AlignCenter);
    chartsPlaceholder->setStyleSheet("QLabel { color: gray; font-size: 14px; }");
    mainLayout->addWidget(chartsPlaceholder);
    
    // Set the main layout
    setLayout(mainLayout);
}

void NetworkCharts::updateConnectionStats(const QMap<QString, quint64> &stats)
{
    if (m_totalConnectionsLabel) {
        m_totalConnectionsLabel->setText(QString::number(stats.value("Total Connections", 0)));
    }
    
    if (m_tcpConnectionsLabel) {
        m_tcpConnectionsLabel->setText(QString::number(stats.value("TCP Connections", 0)));
    }
    
    if (m_udpConnectionsLabel) {
        m_udpConnectionsLabel->setText(QString::number(stats.value("UDP Connections", 0)));
    }
    
    if (m_bytesReceivedLabel) {
        m_bytesReceivedLabel->setText(formatByteSize(stats.value("Total Bytes Received", 0)));
    }
    
    if (m_bytesSentLabel) {
        m_bytesSentLabel->setText(formatByteSize(stats.value("Total Bytes Sent", 0)));
    }
}

QString NetworkCharts::formatByteSize(quint64 bytes) const
{
    constexpr quint64 KB = 1024;
    constexpr quint64 MB = 1024 * KB;
    constexpr quint64 GB = 1024 * MB;
    constexpr quint64 TB = 1024 * GB;
    
    if (bytes < KB) {
        return QString("%1 B").arg(bytes);
    } else if (bytes < MB) {
        return QString("%1 KB").arg(bytes / static_cast<double>(KB), 0, 'f', 2);
    } else if (bytes < GB) {
        return QString("%1 MB").arg(bytes / static_cast<double>(MB), 0, 'f', 2);
    } else if (bytes < TB) {
        return QString("%1 GB").arg(bytes / static_cast<double>(GB), 0, 'f', 2);
    } else {
        return QString("%1 TB").arg(bytes / static_cast<double>(TB), 0, 'f', 2);
    }
}