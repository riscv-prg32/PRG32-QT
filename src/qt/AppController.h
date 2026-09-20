#pragma once
#include "InputState.h"
#include "Runtime.h"
#include <QByteArray>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QPointer>
#include <memory>
class FrameItem;class GamepadBackend;class QtAudioEngine;
class AppController:public QObject{
 Q_OBJECT
 Q_PROPERTY(QString status READ status NOTIFY statusChanged)
 Q_PROPERTY(bool controllerConnected READ controllerConnected NOTIFY controllerChanged)
 Q_PROPERTY(QString controllerName READ controllerName NOTIFY controllerChanged)
 Q_PROPERTY(int ledR READ ledR NOTIFY ledChanged) Q_PROPERTY(int ledG READ ledG NOTIFY ledChanged) Q_PROPERTY(int ledB READ ledB NOTIFY ledChanged) Q_PROPERTY(double ledIntensity READ ledIntensity NOTIFY ledChanged)
 Q_PROPERTY(bool running READ running NOTIFY runningChanged)
public:
 explicit AppController(QObject*p=nullptr);~AppController()override;
 QString status()const{return status_;}bool controllerConnected()const;QString controllerName()const;int ledR()const{return led_.r;}int ledG()const{return led_.g;}int ledB()const{return led_.b;}double ledIntensity()const{return led_.intensity;}bool running()const{return running_;}
 Q_INVOKABLE bool loadBytes(const QByteArray&,const QString&suggestedName={});Q_INVOKABLE bool loadFile(const QUrl&);Q_INVOKABLE void stop();Q_INVOKABLE void setButton(int,bool);Q_INVOKABLE void setDirectional(int);Q_INVOKABLE void setKeyboardButton(int,bool);Q_INVOKABLE void clearKeyboard();Q_INVOKABLE void attachFrame(QObject*);Q_INVOKABLE void playStartupTone();
signals:void statusChanged();void controllerChanged();void ledChanged();void runningChanged();
private:
 void setStatus(QString);void saveCartridge(const QByteArray&,const QString&);void updateFrame();prg32::Runtime rt_;QTimer timer_;InputState input_;QString status_;QPointer<FrameItem> frame_;GamepadBackend*gamepad_=nullptr;std::unique_ptr<QtAudioEngine>audio_;prg32::RGBState led_{};bool running_=false;
};
