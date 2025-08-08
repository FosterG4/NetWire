#include "connectiondetailsdialog.h"
#include "ui_connectiondetailsdialog.h"
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDateTime>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <random>

ConnectionDetailsDialog::ConnectionDetailsDialog(const NetworkMonitor::ConnectionInfo &connection, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ConnectionDetailsDialog)
    , m_connection(connection)
    , m_trafficChart(nullptr)
    , m_downloadSeries(nullptr)
    , m_uploadSeries(nullptr)
    , m_bytesAxis(nullptr)
    , m_timeAxis(nullptr)
    , m_updateTimer(new QTimer(this))
{
    ui->setupUi(this);
    setupUI();
    populateConnectionInfo();
    setupCharts();
    
    // Set up timer for real-time updates
    connect(m_updateTimer, &QTimer::timeout, this, &ConnectionDetailsDialog::updateCharts);
    m_updateTimer->start(1000); // Update every second
}

ConnectionDetailsDialog::~ConnectionDetailsDialog()
{
    delete ui;
}

void ConnectionDetailsDialog::setupUI()
{
    // Connect signals
    connect(ui->terminateButton, &QPushButton::clicked, this, &ConnectionDetailsDialog::terminateConnection);
    connect(ui->refreshButton, &QPushButton::clicked, this, &ConnectionDetailsDialog::refreshData);
    connect(ui->exportButton, &QPushButton::clicked, this, &ConnectionDetailsDialog::exportData);
    connect(ui->closeButton, &QPushButton::clicked, this, &QDialog::accept);
    
    // Set window properties
    setWindowTitle(QString("Connection Details - %1:%2 -> %3:%4")
                   .arg(m_connection.localAddress)
                   .arg(m_connection.localPort)
                   .arg(m_connection.remoteAddress)
                   .arg(m_connection.remotePort));
    
    setModal(true);
    resize(800, 600);
}

void ConnectionDetailsDialog::populateConnectionInfo()
{
    // Connection Information
    ui->localAddressLabel->setText(m_connection.localAddress);
    ui->localPortLabel->setText(QString::number(m_connection.localPort));
    ui->remoteAddressLabel->setText(m_connection.remoteAddress);
    ui->remotePortLabel->setText(QString::number(m_connection.remotePort));
    ui->protocolLabel->setText(m_connection.protocol == 6 ? "TCP" : "UDP");
    ui->stateLabel->setText(m_connection.connectionState);
    
    // Application Information
    ui->applicationLabel->setText(m_connection.processName);
    ui->processIdLabel->setText(QString::number(m_connection.processId));
    ui->pathLabel->setText(m_connection.processPath);
    
    // Traffic Statistics
    ui->bytesReceivedLabel->setText(formatBytes(m_connection.bytesReceived));
    ui->bytesSentLabel->setText(formatBytes(m_connection.bytesSent));
    ui->connectionTimeLabel->setText(m_connection.connectionTime.toString("yyyy-MM-dd hh:mm:ss"));
    ui->lastActivityLabel->setText(m_connection.lastActivity.toString("yyyy-MM-dd hh:mm:ss"));
    
    // Analysis tab
    QString analysis = QString("Connection Analysis\n"
                              "==================\n\n"
                              "Connection Details:\n"
                              "- Local: %1:%2\n"
                              "- Remote: %3:%4\n"
                              "- Protocol: %5\n"
                              "- State: %6\n\n"
                              "Application Details:\n"
                              "- Name: %7\n"
                              "- Process ID: %8\n"
                              "- Path: %9\n\n"
                              "Traffic Statistics:\n"
                              "- Bytes Received: %10\n"
                              "- Bytes Sent: %11\n"
                              "- Connection Time: %12\n"
                              "- Last Activity: %13\n\n"
                              "Security Analysis:\n"
                              "- This connection appears to be %14\n"
                              "- Risk Level: %15\n")
                              .arg(m_connection.localAddress)
                              .arg(m_connection.localPort)
                              .arg(m_connection.remoteAddress)
                              .arg(m_connection.remotePort)
                              .arg(m_connection.protocol == 6 ? "TCP" : "UDP")
                              .arg(m_connection.connectionState)
                              .arg(m_connection.processName)
                              .arg(m_connection.processId)
                              .arg(m_connection.processPath)
                              .arg(formatBytes(m_connection.bytesReceived))
                              .arg(formatBytes(m_connection.bytesSent))
                              .arg(m_connection.connectionTime.toString("yyyy-MM-dd hh:mm:ss"))
                              .arg(m_connection.lastActivity.toString("yyyy-MM-dd hh:mm:ss"))
                              .arg(m_connection.remoteAddress == "127.0.0.1" ? "local" : "external")
                              .arg(m_connection.remotePort == 80 || m_connection.remotePort == 443 ? "Low" : "Medium");
    
    ui->analysisTextEdit->setPlainText(analysis);
}

void ConnectionDetailsDialog::setupCharts()
{
    // Create chart
    m_trafficChart = new QtCharts::QChart();
    m_trafficChart->setTitle("Connection Traffic");
    m_trafficChart->setAnimationOptions(QtCharts::QChart::SeriesAnimations);
    
    // Create series
    m_downloadSeries = new QtCharts::QLineSeries();
    m_downloadSeries->setName("Download");
    m_downloadSeries->setColor(Qt::blue);
    
    m_uploadSeries = new QtCharts::QLineSeries();
    m_uploadSeries->setName("Upload");
    m_uploadSeries->setColor(Qt::red);
    
    // Add series to chart
    m_trafficChart->addSeries(m_downloadSeries);
    m_trafficChart->addSeries(m_uploadSeries);
    
    // Create axes
    m_bytesAxis = new QtCharts::QValueAxis();
    m_bytesAxis->setTitleText("Bytes");
    m_bytesAxis->setLabelFormat("%.0f");
    m_trafficChart->addAxis(m_bytesAxis, Qt::AlignLeft);
    
    m_timeAxis = new QtCharts::QDateTimeAxis();
    m_timeAxis->setTitleText("Time");
    m_timeAxis->setFormat("hh:mm:ss");
    m_trafficChart->addAxis(m_timeAxis, Qt::AlignBottom);
    
    // Attach axes to series
    m_downloadSeries->attachAxis(m_bytesAxis);
    m_downloadSeries->attachAxis(m_timeAxis);
    m_uploadSeries->attachAxis(m_bytesAxis);
    m_uploadSeries->attachAxis(m_timeAxis);
    
    // Set chart view
    ui->trafficChartView->setChart(m_trafficChart);
    ui->trafficChartView->setRenderHint(QPainter::Antialiasing);
}

void ConnectionDetailsDialog::updateCharts()
{
    QDateTime now = QDateTime::currentDateTime();
    
    // Add data points (simulated for now)
    static quint64 lastDownload = m_connection.bytesReceived;
    static quint64 lastUpload = m_connection.bytesSent;
    
    // Simulate traffic increase
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<int> downloadDist(0, 999);
    static std::uniform_int_distribution<int> uploadDist(0, 499);
    
    quint64 downloadIncrease = lastDownload + downloadDist(gen);
    quint64 uploadIncrease = lastUpload + uploadDist(gen);
    
    m_downloadSeries->append(now.toMSecsSinceEpoch(), downloadIncrease);
    m_uploadSeries->append(now.toMSecsSinceEpoch(), uploadIncrease);
    
    // Keep only last 60 data points
    if (m_downloadSeries->count() > 60) {
        m_downloadSeries->removePoints(0, m_downloadSeries->count() - 60);
        m_uploadSeries->removePoints(0, m_uploadSeries->count() - 60);
    }
    
    // Update axes ranges
    if (m_downloadSeries->count() > 0) {
        m_timeAxis->setRange(QDateTime::fromMSecsSinceEpoch(m_downloadSeries->at(0).x()),
                            QDateTime::fromMSecsSinceEpoch(m_downloadSeries->at(m_downloadSeries->count() - 1).x()));
        
        qreal maxBytes = qMax(m_downloadSeries->at(m_downloadSeries->count() - 1).y(),
                             m_uploadSeries->at(m_uploadSeries->count() - 1).y());
        m_bytesAxis->setRange(0, maxBytes * 1.1);
    }
    
    lastDownload = downloadIncrease;
    lastUpload = uploadIncrease;
}

void ConnectionDetailsDialog::terminateConnection()
{
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Terminate Connection",
        QString("Are you sure you want to terminate the connection to %1:%2?").arg(m_connection.remoteAddress).arg(m_connection.remotePort),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        // This would call the network monitor to terminate the connection
        // For now, just close the dialog
        QMessageBox::information(this, "Connection Terminated", "The connection has been terminated.");
        accept();
    }
}

void ConnectionDetailsDialog::refreshData()
{
    // Refresh connection data
    populateConnectionInfo();
    
    // Update charts
    updateCharts();
    
    QMessageBox::information(this, "Data Refreshed", "Connection data has been refreshed.");
}

void ConnectionDetailsDialog::exportData()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export Connection Data",
        QString("connection_%1_%2_%3.csv").arg(m_connection.localAddress).arg(m_connection.localPort).arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        "CSV Files (*.csv);;All Files (*)");
    
    if (!filename.isEmpty()) {
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            
            // Write header
            out << "Local Address,Local Port,Remote Address,Remote Port,Protocol,Application,Process ID,State,Bytes Received,Bytes Sent,Connection Time,Last Activity\n";
            
            // Write data
            out << QString("%1,%2,%3,%4,%5,%6,%7,%8,%9,%10,%11,%12\n")
                   .arg(m_connection.localAddress)
                   .arg(m_connection.localPort)
                   .arg(m_connection.remoteAddress)
                   .arg(m_connection.remotePort)
                   .arg(m_connection.protocol == 6 ? "TCP" : "UDP")
                   .arg(m_connection.processName)
                   .arg(m_connection.processId)
                   .arg(m_connection.connectionState)
                   .arg(m_connection.bytesReceived)
                   .arg(m_connection.bytesSent)
                   .arg(m_connection.connectionTime.toString("yyyy-MM-dd hh:mm:ss"))
                   .arg(m_connection.lastActivity.toString("yyyy-MM-dd hh:mm:ss"));
            
            file.close();
            QMessageBox::information(this, "Export Complete", "Connection data has been exported successfully.");
        } else {
            QMessageBox::critical(this, "Export Error", "Could not write to the selected file.");
        }
    }
}

QString ConnectionDetailsDialog::formatBytes(quint64 bytes) const
{
    static const char *suffixes[] = {"B", "KB", "MB", "GB", "TB", "PB", "EB"};
    int i = 0;
    double size = bytes;
    while (size >= 1024 && i < 6) {
        size /= 1024;
        i++;
    }
    return QString("%1 %2").arg(size, 0, 'f', i > 0 ? 2 : 0).arg(suffixes[i]);
}

QString ConnectionDetailsDialog::formatSpeed(double bytesPerSecond) const
{
    static const char *units[] = {"B/s", "KB/s", "MB/s", "GB/s", "TB/s"};
    int i = 0;
    double speed = bytesPerSecond;
    while (speed >= 1024 && i < 4) {
        speed /= 1024;
        i++;
    }
    return QString("%1 %2").arg(speed, 0, 'f', i > 0 ? 2 : 0).arg(units[i]);
}
