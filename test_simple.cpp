#include <QApplication>
#include <QMainWindow>
#include <QLabel>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    QMainWindow window;
    window.setWindowTitle("Test Window");
    window.resize(400, 300);
    
    QLabel *label = new QLabel("Hello World!", &window);
    window.setCentralWidget(label);
    
    window.show();
    
    return app.exec();
}
