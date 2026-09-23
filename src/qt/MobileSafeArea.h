#pragma once

#include <QObject>

/** Exposes native mobile safe-area insets to QML in device-independent pixels. */
class MobileSafeArea final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int left READ left NOTIFY marginsChanged)
    Q_PROPERTY(int top READ top NOTIFY marginsChanged)
    Q_PROPERTY(int right READ right NOTIFY marginsChanged)
    Q_PROPERTY(int bottom READ bottom NOTIFY marginsChanged)

  public:
    explicit MobileSafeArea(QObject* parent = nullptr);
    int left() const;
    int top() const;
    int right() const;
    int bottom() const;
    Q_INVOKABLE void refresh();

  signals:
    void marginsChanged();

  private:
    int left_ = 0;
    int top_ = 0;
    int right_ = 0;
    int bottom_ = 0;
};
