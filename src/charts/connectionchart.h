#ifndef CONNECTIONCHART_H
#define CONNECTIONCHART_H

#include <QWidget>

class ConnectionChart : public QWidget
{
    Q_OBJECT

public:
    explicit ConnectionChart(QWidget *parent = nullptr);

private:
    void setupLayout();
};

#endif // CONNECTIONCHART_H 