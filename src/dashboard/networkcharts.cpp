#include "networkcharts.h"
#include <QVBoxLayout>
#include <QLabel>

NetworkCharts::NetworkCharts(QWidget *parent)
    : QWidget(parent)
{
    setupLayout();
}

void NetworkCharts::setupLayout()
{
    auto layout = new QVBoxLayout(this);
    auto placeholder = new QLabel("Network Charts - Coming Soon", this);
    placeholder->setAlignment(Qt::AlignCenter);
    placeholder->setStyleSheet("QLabel { color: gray; font-size: 14px; }");
    layout->addWidget(placeholder);
    setLayout(layout);
} 