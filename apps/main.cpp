#include <QApplication>
#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("InfraredDetailEnhancement");
    app.setOrganizationName("DeepMindLab");

    MainWindow window;
    window.show();

    return app.exec();
}
