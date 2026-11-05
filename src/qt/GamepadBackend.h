#pragma once
#include <QObject>
#include <QString>
#include <cstdint>

class GamepadBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(QString name READ name NOTIFY connectedChanged)
  public:
    explicit GamepadBackend(QObject* parent = nullptr) : QObject(parent) {
    }
    bool connected() const {
        return connected_;
    }
    QString name() const {
        return name_;
    }
    void setState(bool connected, QString name) {
        if (connected_ == connected && name_ == name)
            return;
        connected_ = connected;
        name_ = std::move(name);
        emit connectedChanged();
    }
    void publish(std::uint32_t mask) {
        emit inputChanged(mask);
    }
    /**
     * Report the controller's left stick for the analog ABI (#140-#143).
     *
     * @param available true when the attached controller has a stick.
     * @param unitX horizontal position, -1 (left) to 1 (right).
     * @param unitY vertical position, -1 (up) to 1 (down): screen-down, like the firmware's axis.
     */
    void publishAnalog(bool available, double unitX, double unitY) {
        emit analogChanged(available, toTilt(unitX), toTilt(unitY));
    }
  signals:
    void inputChanged(quint32 mask);
    /** Stick tilt in -127..127 per axis (PRG32 axis full scale), before the firmware's deadzone. */
    void analogChanged(bool available, int tiltX, int tiltY);
    void connectedChanged();

  private:
    /** -1..1 to the PRG32 axis full scale of 127, rounded to the nearest step. */
    static int toTilt(double unit) {
        constexpr double FullScale = 127.0;
        const double clamped = unit < -1.0 ? -1.0 : (unit > 1.0 ? 1.0 : unit);
        return int(clamped * FullScale + (clamped < 0 ? -0.5 : 0.5));
    }
    bool connected_ = false;
    QString name_;
};
GamepadBackend* createGamepadBackend(QObject* parent = nullptr);
