#include "AppController.h"
#include "FrameItem.h"
#include "GamepadBackend.h"
#include "QtAudioEngine.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
AppController::AppController(QObject*p):QObject(p),audio_(std::make_unique<QtAudioEngine>()){
 rt_.setAudioSink(audio_.get());rt_.setLEDCallback([this](prg32::RGBState v){led_=v;emit ledChanged();});
 gamepad_=createGamepadBackend(this);connect(gamepad_,&GamepadBackend::inputChanged,this,[this](quint32 mask){input_.replace(InputState::Controller0,mask);});connect(gamepad_,&GamepadBackend::connectedChanged,this,&AppController::controllerChanged);
 timer_.setTimerType(Qt::PreciseTimer);timer_.setInterval(16);connect(&timer_,&QTimer::timeout,this,[this]{std::string e;if(!rt_.frame(input_.merged(),e)){timer_.stop();running_=false;emit runningChanged();setStatus(QString::fromStdString(e));return;}led_.intensity*=.90;emit ledChanged();updateFrame();});
}
AppController::~AppController(){rt_.stop();}
void AppController::setStatus(QString s){if(status_==s)return;status_=std::move(s);emit statusChanged();}bool AppController::controllerConnected()const{return gamepad_&&gamepad_->connected();}QString AppController::controllerName()const{return gamepad_?gamepad_->name():QString();}
void AppController::updateFrame(){if(frame_)frame_->setFrame(rt_.framebuffer().rgb565Pixels());}
bool AppController::loadBytes(const QByteArray&d,const QString&suggestedName){std::string e;auto c=prg32::Cartridge::parse(std::span<const uint8_t>((const uint8_t*)d.constData(),size_t(d.size())),e);if(!c){setStatus(QString::fromStdString(e));return false;}rt_.stop();if(!rt_.load(*c,e)||!rt_.init(e)){setStatus(QString::fromStdString(e));return false;}updateFrame();timer_.start();running_=true;emit runningChanged();QString n=suggestedName.isEmpty()?QString::fromStdString(c->name()):suggestedName;saveCartridge(d,n);setStatus(QString("Running %1").arg(QString::fromStdString(c->name())));return true;}
bool AppController::loadFile(const QUrl&u){QString path=u.toLocalFile();QFile f(path);if(!f.open(QIODevice::ReadOnly)){setStatus("Cannot open cartridge file");return false;}return loadBytes(f.readAll(),QFileInfo(path).completeBaseName());}
void AppController::saveCartridge(const QByteArray&d,const QString&name){QString root=QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);if(root.isEmpty())return;QDir dir(root);dir.mkpath("PRG32Cartridges");QString safe=name;safe.replace('/','_').replace('\\','_');QFile f(dir.filePath("PRG32Cartridges/"+safe+".prg32"));if(f.open(QIODevice::WriteOnly|QIODevice::Truncate))f.write(d);}
void AppController::stop(){timer_.stop();rt_.stop();input_=InputState{};if(running_){running_=false;emit runningChanged();}}
void AppController::setButton(int m,bool down){input_.set(InputState::Ui,uint32_t(m),down);}void AppController::setDirectional(int m){input_.replace(InputState::Ui,(input_.value(InputState::Ui)&~0x0fu)|(uint32_t(m)&0x0f));}
void AppController::setKeyboardButton(int m,bool d){input_.set(InputState::Keyboard,uint32_t(m),d);}void AppController::clearKeyboard(){input_.clear(InputState::Keyboard);}void AppController::attachFrame(QObject*o){frame_=qobject_cast<FrameItem*>(o);updateFrame();}void AppController::playStartupTone(){audio_->tone(523.25,120,180);QTimer::singleShot(130,this,[this]{audio_->tone(659.25,120,180);});QTimer::singleShot(260,this,[this]{audio_->tone(783.99,660,180);});}
