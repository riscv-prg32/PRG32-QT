#pragma once
#include <QImage>
#include <QQuickPaintedItem>
#include <cstdint>
#include <vector>
class FrameItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(bool statusBarsEnabled READ statusBarsEnabled WRITE setStatusBarsEnabled NOTIFY
                   statusBarsEnabledChanged)
    Q_PROPERTY(QString statusText READ statusText WRITE setStatusText NOTIFY statusTextChanged)
    Q_PROPERTY(
        int framesPerSecond READ framesPerSecond WRITE setFramesPerSecond NOTIFY framesPerSecondChanged)
  public:
    explicit FrameItem(QQuickItem* parent = nullptr) : QQuickPaintedItem(parent) {
    }
    void paint(QPainter* painter) override;
    void setFrame(const std::vector<uint16_t>& pixels);
    bool statusBarsEnabled() const {
        return statusBarsEnabled_;
    }
    QString statusText() const {
        return statusText_;
    }
    int framesPerSecond() const {
        return framesPerSecond_;
    }
    void setStatusBarsEnabled(bool enabled);
    void setStatusText(const QString& text);
    void setFramesPerSecond(int framesPerSecond);
  signals:
    void statusBarsEnabledChanged();
    void statusTextChanged();
    void framesPerSecondChanged();

  private:
    QImage image_;
    bool statusBarsEnabled_ = false;
    QString statusText_ = "PRG32";
    int framesPerSecond_ = 0;
};
