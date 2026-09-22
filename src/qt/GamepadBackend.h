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
  signals:
    void inputChanged(quint32 mask);
    void connectedChanged();

  private:
    bool connected_ = false;
    QString name_;
};
GamepadBackend* createGamepadBackend(QObject* parent = nullptr);
