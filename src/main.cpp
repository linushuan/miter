#include <QApplication>
#include <QStringList>
#include <QIcon>
#include <QPixmap>
#include <QPainter>
#include <QFont>
#include "mainwindow.h"

static QIcon createFallbackAppIcon()
{
    QIcon themeIcon = QIcon::fromTheme("accessories-text-editor");
    if (!themeIcon.isNull())
        return themeIcon;

    QPixmap pm(128, 128);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setBrush(QColor("#2a6f97"));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(8, 8, 112, 112, 20, 20);

    QFont f;
    f.setBold(true);
    f.setPointSize(46);
    p.setFont(f);
    p.setPen(QColor("#f7f9fb"));
    p.drawText(pm.rect(), Qt::AlignCenter, "M");
    return QIcon(pm);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("mded");
    app.setApplicationVersion("0.2.0");
    QApplication::setDesktopFileName("mded");
    app.setWindowIcon(createFallbackAppIcon());

    // Collect file paths from CLI arguments
    QStringList filesToOpen;
    const QStringList args = app.arguments();
    for (int i = 1; i < args.size(); ++i) {
        filesToOpen.append(args.at(i));
    }

    MainWindow window(filesToOpen);
    window.setWindowIcon(app.windowIcon());
    window.show();

    return app.exec();
}
