#include "cli_plus/main_window.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("ZZmazon Studio"));
    application.setOrganizationName(QStringLiteral("ZZmazon"));
    application.setApplicationDisplayName(QStringLiteral("ZZmazon Studio"));

    QFont applicationFont = application.font();
    applicationFont.setFamilies({
        QStringLiteral("Helvetica Neue"),
        QStringLiteral("PingFang SC"),
        QStringLiteral("Noto Sans CJK SC"),
        QStringLiteral("Segoe UI"),
    });
    applicationFont.setPointSize(12);
    application.setFont(applicationFont);

    QFile theme(QStringLiteral(":/cli_plus/theme.qss"));
    if (theme.open(QIODevice::ReadOnly | QIODevice::Text)) {
        application.setStyleSheet(QString::fromUtf8(theme.readAll()));
    }

    zzmazon::MainWindow window;
    window.show();

    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect available = screen->availableGeometry();
        window.move(available.center() - window.rect().center());
    }

    const QStringList arguments = application.arguments();
    const int screenshotIndex = arguments.indexOf(QStringLiteral("--screenshot"));
    if (screenshotIndex >= 0 && screenshotIndex + 1 < arguments.size()) {
        const QString outputPath = QDir::cleanPath(arguments.at(screenshotIndex + 1));
        QTimer::singleShot(650, &window, [&application, &window, outputPath] {
            window.grab().save(outputPath);
            application.quit();
        });
    } else if (arguments.contains(QStringLiteral("--smoke-test"))) {
        QTimer::singleShot(350, &application, &QApplication::quit);
    }

    return application.exec();
}
