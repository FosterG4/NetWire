#ifndef NETWORKCHARTS_H
#define NETWORKCHARTS_H

#include <QWidget>

class NetworkCharts : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkCharts(QWidget *parent = nullptr);

private:
    void setupLayout();
};

#endif // NETWORKCHARTS_H 