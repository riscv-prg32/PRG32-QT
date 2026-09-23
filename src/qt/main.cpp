#include "AppController.h"
#include "FrameItem.h"
#include "MobileSafeArea.h"
#include "RasterImageItem.h"
#include "StoreClient.h"
#include "WebApiServer.h"
#include <QGuiApplication>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QResource>
#include <QTimer>
int main(int argc, char** argv) {
    Q_INIT_RESOURCE(prg32qt_assets);
    QGuiApplication app(argc, argv);
    app.setOrganizationName("riscv-prg32");
    app.setApplicationName("PRG32");
    app.setApplicationVersion(PRG32QT_VERSION);
    qmlRegisterType<FrameItem>("PRG32Qt", 1, 0, "PRG32Frame");
    qmlRegisterType<RasterImageItem>("PRG32Qt", 1, 0, "PRG32Image");
    StoreClient store;
    AppController controller;
    MobileSafeArea mobileSafeArea;
    QString screenshotPath;
    QString screenshotPage;
    QString screenshotCartridge;
    for (int argument = 1; argument < argc; ++argument) {
        QString value = QString::fromLocal8Bit(argv[argument]);
        if (value == "--documentation-screenshot" && argument + 2 < argc) {
            screenshotPage = QString::fromLocal8Bit(argv[++argument]);
            screenshotPath = QString::fromLocal8Bit(argv[++argument]);
        } else if (value == "--cartridge" && argument + 1 < argc) {
            screenshotCartridge = QString::fromLocal8Bit(argv[++argument]);
        }
    }
    if (!screenshotCartridge.isEmpty())
        controller.loadFile(QUrl::fromLocalFile(screenshotCartridge));
    WebApiServer web(&controller);
    QQmlApplicationEngine e;
    e.rootContext()->setContextProperty("storeClient", &store);
    e.rootContext()->setContextProperty("appController", &controller);
    e.rootContext()->setContextProperty("mobileSafeArea", &mobileSafeArea);
    e.rootContext()->setContextProperty("documentationPage", screenshotPage);
#ifdef PRG32QT_TV_MODE
    e.rootContext()->setContextProperty("tvPlatform", true);
#else
    e.rootContext()->setContextProperty("tvPlatform", false);
#endif
    e.loadFromModule("PRG32Qt", "Main");
    if (e.rootObjects().isEmpty())
        return 1;
    if (!screenshotPath.isEmpty()) {
        QTimer::singleShot(2500, &app, [&app, &e, screenshotPath] {
            auto* window = qobject_cast<QQuickWindow*>(e.rootObjects().constFirst());
            if (!window || !window->grabWindow().save(screenshotPath))
                app.exit(2);
            else
                app.quit();
        });
    }
    return app.exec();
}
