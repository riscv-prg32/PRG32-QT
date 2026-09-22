#include "AppController.h"
#include "FrameItem.h"
#include "RasterImageItem.h"
#include "StoreClient.h"
#include "WebApiServer.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QResource>
int main(int argc, char** argv) {
    Q_INIT_RESOURCE(prg32qt_assets);
    QGuiApplication app(argc, argv);
    app.setOrganizationName("riscv-prg32");
    app.setApplicationName("PRG32");
    qmlRegisterType<FrameItem>("PRG32Qt", 1, 0, "PRG32Frame");
    qmlRegisterType<RasterImageItem>("PRG32Qt", 1, 0, "PRG32Image");
    StoreClient store;
    AppController controller;
    WebApiServer web(&controller);
    QQmlApplicationEngine e;
    e.rootContext()->setContextProperty("storeClient", &store);
    e.rootContext()->setContextProperty("appController", &controller);
    e.loadFromModule("PRG32Qt", "Main");
    if (e.rootObjects().isEmpty())
        return 1;
    return app.exec();
}
