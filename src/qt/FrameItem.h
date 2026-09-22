#pragma once
#include <QImage>
#include <QQuickPaintedItem>
#include <cstdint>
#include <vector>
class FrameItem : public QQuickPaintedItem {
    Q_OBJECT
  public:
    explicit FrameItem(QQuickItem* parent = nullptr) : QQuickPaintedItem(parent) {
    }
    void paint(QPainter* painter) override;
    void setFrame(const std::vector<uint16_t>& pixels);

  private:
    QImage image_;
};
