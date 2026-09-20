#include "AppController.h"
#include "FrameItem.h"
#include "StoreClient.h"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
int main(int argc,char**argv){QGuiApplication app(argc,argv);app.setOrganizationName("riscv-prg32");app.setApplicationName("PRG32");qmlRegisterType<FrameItem>("PRG32Qt",1,0,"PRG32Frame");StoreClient store;AppController controller;QQmlApplicationEngine e;e.rootContext()->setContextProperty("storeClient",&store);e.rootContext()->setContextProperty("appController",&controller);e.loadFromModule("PRG32Qt","Main");if(e.rootObjects().isEmpty())return 1;return app.exec();}
