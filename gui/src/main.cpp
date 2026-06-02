#include <QApplication>
#include <QPalette>
#include "MainWindow.h"

static void applyDarkPalette(QApplication &app)
{
    app.setStyle("Fusion");

    QPalette p;
    p.setColor(QPalette::Window,          QColor( 30,  30,  30));
    p.setColor(QPalette::WindowText,      QColor(220, 220, 220));
    p.setColor(QPalette::Base,            QColor( 20,  20,  20));
    p.setColor(QPalette::AlternateBase,   QColor( 40,  40,  40));
    p.setColor(QPalette::ToolTipBase,     QColor( 50,  50,  50));
    p.setColor(QPalette::ToolTipText,     QColor(220, 220, 220));
    p.setColor(QPalette::Text,            QColor(220, 220, 220));
    p.setColor(QPalette::Button,          QColor( 45,  45,  45));
    p.setColor(QPalette::ButtonText,      QColor(220, 220, 220));
    p.setColor(QPalette::BrightText,      Qt::red);
    p.setColor(QPalette::Link,            QColor( 42, 130, 218));
    p.setColor(QPalette::Highlight,       QColor( 42, 130, 218));
    p.setColor(QPalette::HighlightedText, Qt::black);
    p.setColor(QPalette::Disabled, QPalette::Text,       QColor(100, 100, 100));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(100, 100, 100));
    app.setPalette(p);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    applyDarkPalette(app);

    MainWindow window;
    window.show();

    return app.exec();
}