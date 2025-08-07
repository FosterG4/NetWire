#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QApplication>
#include <QDebug>

class MinimalMainWindow : public QMainWindow
{
public:
    explicit MinimalMainWindow(QWidget *parent = nullptr) : QMainWindow(parent)
    {
        qDebug() << "MinimalMainWindow constructor called";
        
        setWindowTitle("NetWire - Minimal Test");
        resize(800, 600);
        
        // Create central widget
        QWidget *centralWidget = new QWidget(this);
        setCentralWidget(centralWidget);
        
        // Create layout
        QVBoxLayout *layout = new QVBoxLayout(centralWidget);
        
        // Add a label
        QLabel *label = new QLabel("NetWire is running!", centralWidget);
        layout->addWidget(label);
        
        // Add a button
        QPushButton *button = new QPushButton("Test Button", centralWidget);
        layout->addWidget(button);
        
        qDebug() << "MinimalMainWindow setup complete";
    }
    
    ~MinimalMainWindow()
    {
        qDebug() << "MinimalMainWindow destructor called";
    }
};

// MOC will be handled automatically by CMake
