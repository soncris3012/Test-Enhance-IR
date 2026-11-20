#include <QApplication>
#include <QPixmap>
#include <iostream>
#include <string>
#include "MainWindow.hpp"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("InfraredDetailEnhancement");
    app.setOrganizationName("DeepMindLab");

    MainWindow window;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--screenshot" && i + 1 < argc) {
            window.resize(1100, 720);
            window.show();
            app.processEvents();
            QPixmap pm = window.grab();
            pm.save(QString::fromUtf8(argv[i + 1]));
            std::cout << "Saved UI screenshot to: " << argv[i + 1] << std::endl;
            return 0;
        }
    }

    window.show();
    return app.exec();
}
