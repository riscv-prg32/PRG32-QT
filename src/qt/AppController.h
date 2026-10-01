#pragma once
#include "InputState.h"
#include "Runtime.h"
#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <memory>
class FrameItem;
class GamepadBackend;
class QtAudioEngine;
class QtMultiplayerService;
class AppController : public QObject {
    Q_OBJECT
    // clang-format off
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool controllerConnected READ controllerConnected NOTIFY controllerChanged)
    Q_PROPERTY(QString controllerName READ controllerName NOTIFY controllerChanged)
    Q_PROPERTY(int ledR READ ledR NOTIFY ledChanged)
    Q_PROPERTY(int ledG READ ledG NOTIFY ledChanged)
    Q_PROPERTY(int ledB READ ledB NOTIFY ledChanged)
    Q_PROPERTY(double ledIntensity READ ledIntensity NOTIFY ledChanged)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(QString deviceIp READ deviceIp NOTIFY networkChanged)
    Q_PROPERTY(QString webApiUrl READ webApiUrl NOTIFY networkChanged)
    Q_PROPERTY(QString cartridgeName READ cartridgeName NOTIFY cartridgeChanged)
    Q_PROPERTY(bool performanceAvailable READ performanceAvailable NOTIFY cartridgeChanged)
    Q_PROPERTY(QString performanceState READ performanceState NOTIFY performanceChanged)
    Q_PROPERTY(QString preferredOrientation READ preferredOrientation WRITE setPreferredOrientation NOTIFY
                   displayPreferencesChanged)
    Q_PROPERTY(bool fullScreen READ fullScreen WRITE setFullScreen NOTIFY displayPreferencesChanged)
    Q_PROPERTY(QString performanceMode READ performanceMode WRITE setPerformanceMode NOTIFY performanceModeChanged)
    Q_PROPERTY(int framesPerSecond READ framesPerSecond NOTIFY frameStatsChanged)
    Q_PROPERTY(bool statusBarsEnabled READ statusBarsEnabled WRITE setStatusBarsEnabled NOTIFY
                   displayPreferencesChanged)
    Q_PROPERTY(bool debugEnabled READ debugEnabled WRITE setDebugEnabled NOTIFY debugChanged)
    Q_PROPERTY(QString debugAssembly READ debugAssembly NOTIFY debugChanged)
    Q_PROPERTY(QString debugRegisters READ debugRegisters NOTIFY debugChanged)
    Q_PROPERTY(QString debugMemory READ debugMemory NOTIFY debugChanged)
    Q_PROPERTY(QString debugPhase READ debugPhase NOTIFY debugChanged)
    Q_PROPERTY(QString debugView READ debugView NOTIFY debugChanged)
    Q_PROPERTY(double debugSpeed READ debugSpeed WRITE setDebugSpeed NOTIFY debugChanged)
    // clang-format on

  public:
    explicit AppController(QObject* p = nullptr);
    ~AppController() override;
    QString status() const {
        return status_;
    }
    bool controllerConnected() const;
    QString controllerName() const;
    int ledR() const {
        return led_.r;
    }
    int ledG() const {
        return led_.g;
    }
    int ledB() const {
        return led_.b;
    }
    double ledIntensity() const {
        return led_.intensity;
    }
    bool running() const {
        return running_;
    }
    bool paused() const {
        return paused_;
    }
    QString deviceIp() const {
        return deviceIp_;
    }
    QString webApiUrl() const {
        return deviceIp_.isEmpty() ? QString() : QString("http://%1:8080").arg(deviceIp_);
    }
    QString cartridgeName() const {
        return cartridgeName_;
    }
    bool performanceAvailable() const {
        return performanceAvailable_;
    }
    QString performanceState() const;
    QString preferredOrientation() const {
        return preferredOrientation_;
    }
    bool fullScreen() const {
        return fullScreen_;
    }
    QString performanceMode() const {
        return performanceMode_;
    }
    bool statusBarsEnabled() const {
        return statusBarsEnabled_;
    }
    int framesPerSecond() const {
        return framesPerSecond_;
    }
    bool debugEnabled() const {
        return debugEnabled_;
    }
    QString debugAssembly() const;
    QString debugRegisters() const;
    QString debugMemory() const {
        return debugMemory_;
    }
    QString debugPhase() const;
    QString debugView() const {
        return debugView_;
    }
    double debugSpeed() const {
        return debugSpeed_;
    }
    QJsonObject runtimeJson() const;
    QJsonArray gamesJson() const;
    QJsonObject performanceJson() const;
    QJsonObject memoryJson() const;
    QJsonObject debugJson(uint32_t address = 0, int length = 128) const;
    bool debugCommand(const QJsonObject& command, QString& error);
    QJsonArray scoresJson() const;
    bool submitScore(const QJsonObject&, QString&);
    QByteArray screenshotBmp() const;
    bool uploadSlot(int, const QByteArray&, QString&);
    bool selectSlot(int, QString&);
    Q_INVOKABLE bool loadBytes(const QByteArray&, const QString& suggestedName = {});
    Q_INVOKABLE bool loadFile(const QUrl&);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void resume();
    Q_INVOKABLE void setDebugEnabled(bool enabled);
    Q_INVOKABLE bool debugStep();
    /** Move the disassembly view to init, update, draw, or the live program counter. */
    Q_INVOKABLE void showDebugEntry(const QString& entry);
    /** Set the debugger's continuous-playback rate multiplier. */
    Q_INVOKABLE void setDebugSpeed(double speed);
    Q_INVOKABLE void inspectMemory(const QString& address, int length = 128);
    Q_INVOKABLE void setButton(int, bool);
    Q_INVOKABLE void setDirectional(int);
    Q_INVOKABLE void setKeyboardButton(int, bool);
    Q_INVOKABLE void clearKeyboard();
    Q_INVOKABLE void attachFrame(QObject*);
    Q_INVOKABLE void playStartupTone();
    Q_INVOKABLE void runPerformanceTest();
    /** Point multiplayer rooms at the configured Cartridge Store relay. */
    void setMultiplayerStoreUrl(const QUrl& url);
    /** Select the player layout independently from the physical window orientation. */
    void setPreferredOrientation(const QString& orientation);
    /** Persist whether the desktop player should occupy the full screen. */
    void setFullScreen(bool enabled);
    /** Persist and apply the cartridge-visible performance profile. */
    void setPerformanceMode(const QString& mode);
    /** Persist whether the firmware-style top and bottom status bars are shown. */
    void setStatusBarsEnabled(bool enabled);
  signals:
    void statusChanged();
    void controllerChanged();
    void ledChanged();
    void runningChanged();
    void pausedChanged();
    void networkChanged();
    void cartridgeChanged();
    void performanceChanged();
    void displayPreferencesChanged();
    void performanceModeChanged();
    void frameStatsChanged();
    void debugChanged();

  private:
    void setStatus(QString);
    void saveCartridge(const QByteArray&, const QString&);
    void updateFrame();
    void updateTimerInterval();
    void refreshIp();
    QString slotPath(int) const;
    prg32::Runtime rt_;
    QTimer timer_, ipTimer_;
    InputState input_;
    QString status_, deviceIp_, cartridgeName_;
    QString preferredOrientation_ = "auto";
    QString performanceMode_ = "accurate";
    QByteArray currentBytes_;
    QPointer<FrameItem> frame_;
    GamepadBackend* gamepad_ = nullptr;
    std::unique_ptr<QtAudioEngine> audio_;
    std::unique_ptr<QtMultiplayerService> multiplayer_;
    prg32::RGBState led_{};
    bool running_ = false, paused_ = false, performanceAvailable_ = false;
    bool fullScreen_ = false;
    bool statusBarsEnabled_ = false;
    bool debugEnabled_ = false;
    QString debugMemory_;
    QString debugView_ = "pc";
    uint32_t debugViewAddress_ = 0;
    double debugSpeed_ = 1.0;
    uint64_t frameCount_ = 0;
    QElapsedTimer frameRateTimer_;
    int framesPerSecond_ = 0;
    int frameRateCount_ = 0;
};
