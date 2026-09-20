#pragma once
#include <QQuickPaintedItem>
#include <QImage>
#include <vector>
#include <cstdint>
class FrameItem : public QQuickPaintedItem {
    Q_OBJECT
public:
    explicit FrameItem(QQuickItem *parent = nullptr) : QQuickPaintedItem(parent) {}
    void paint(QPainter *painter) override;
    void setFrame(const std::vector<uint16_t> &pixels);
private:
    QImage image_;
};
