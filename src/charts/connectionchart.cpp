#include "connectionchart.h"
#include <QVBoxLayout>
#include <QLabel>

ConnectionChart::ConnectionChart(QWidget *parent)
    : QWidget(parent)
{
    setupLayout();
}

void ConnectionChart::setupLayout()
{
    auto layout = new QVBoxLayout(this);
    auto placeholder = new QLabel("Connection Chart - Coming Soon", this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("QLabel { color: gray; font-size: 14px; }");
    layout->addWidget(placeholder);
    setLayout(layout);
} 