#ifndef CONNECTIONDETAILSDIALOG_H
#define CONNECTIONDETAILSDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTextEdit>
#include <QTabWidget>
#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include "networkmonitor.h"

QT_BEGIN_NAMESPACE
namespace Ui { class ConnectionDetailsDialog; }
QT_END_NAMESPACE

class ConnectionDetailsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionDetailsDialog(const NetworkMonitor::ConnectionInfo &connection, QWidget *parent = nullptr);
    ~ConnectionDetailsDialog();

private slots:
    void terminateConnection();
    void refreshData();
    void exportData();

private:
    void setupUI();
    void populateConnectionInfo();
    void setupCharts();
    void updateCharts();
    QString formatBytes(quint64 bytes) const;
    QString formatSpeed(double bytesPerSecond) const;

    Ui::ConnectionDetailsDialog *ui;
    NetworkMonitor::ConnectionInfo m_connection;
    QtCharts::QChart *m_trafficChart;
    QtCharts::QLineSeries *m_downloadSeries;
    QtCharts::QLineSeries *m_uploadSeries;
    QtCharts::QValueAxis *m_bytesAxis;
    QtCharts::QDateTimeAxis *m_timeAxis;
    QTimer *m_updateTimer;
};

#endif // CONNECTIONDETAILSDIALOG_H
