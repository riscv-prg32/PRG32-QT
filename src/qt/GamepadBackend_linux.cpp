#include "GamepadBackend.h"
#include <QFileInfo>
#include <QTimer>
#include <array>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <linux/joystick.h>
#include <sys/ioctl.h>
#include <unistd.h>
class LinuxGamepadBackend final : public GamepadBackend {
  public:
    explicit LinuxGamepadBackend(QObject* p = nullptr) : GamepadBackend(p) {
        timer_.setInterval(16);
        connect(&timer_, &QTimer::timeout, this, &LinuxGamepadBackend::poll);
        timer_.start();
        poll();
    }
    ~LinuxGamepadBackend() override {
        if (fd_ >= 0)
            ::close(fd_);
    }

  private:
    void discover() {
        if (fd_ >= 0)
            return;
        for (int i = 0; i < 8; i++) {
            QString path = QString("/dev/input/js%1").arg(i);
            int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
            if (fd < 0)
                continue;
            fd_ = fd;
            char name[128] = {};
            if (ioctl(fd_, JSIOCGNAME(sizeof(name)), name) < 0)
                std::snprintf(name, sizeof(name), "Linux joystick %d", i);
            setState(true, QString::fromLocal8Bit(name));
            break;
        }
    }
    void poll() {
        discover();
        if (fd_ < 0) {
            setState(false, {});
            publish(0);
            return;
        }
        js_event e{};
        bool changed = false;
        while (::read(fd_, &e, sizeof(e)) == sizeof(e)) {
            e.type &= ~JS_EVENT_INIT;
            if (e.type == JS_EVENT_AXIS && e.number < axes_.size()) {
                axes_[e.number] = e.value;
                changed = true;
            } else if (e.type == JS_EVENT_BUTTON && e.number < buttons_.size()) {
                buttons_[e.number] = e.value != 0;
                changed = true;
            }
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ::close(fd_);
            fd_ = -1;
            axes_.fill(0);
            buttons_.fill(false);
            setState(false, {});
            publish(0);
            return;
        }
        if (changed) {
            quint32 m = 0;
            int x = axes_[0], y = axes_[1], hatX = axes_[6], hatY = axes_[7];
            if (x < -12000 || hatX < -12000)
                m |= 1;
            if (x > 12000 || hatX > 12000)
                m |= 2;
            if (y < -12000 || hatY < -12000)
                m |= 4;
            if (y > 12000 || hatY > 12000)
                m |= 8;
            if (buttons_[0])
                m |= 16;
            if (buttons_[1])
                m |= 32;
            if (buttons_[6] || buttons_[7] || buttons_[8] || buttons_[9])
                m |= 64;
            publish(m);
        }
    }
    QTimer timer_;
    int fd_ = -1;
    std::array<int16_t, 16> axes_{};
    std::array<bool, 32> buttons_{};
};
GamepadBackend* createGamepadBackend(QObject* p) {
    return new LinuxGamepadBackend(p);
}
