#pragma once

#include <QImage>
#include <QQuickPaintedItem>

class RasterImageItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
public:
    explicit RasterImageItem(QQuickItem *parent = nullptr);
    QString source() const { return source_; }
    void setSource(const QString &source);
    void paint(QPainter *painter) override;
signals:
    void sourceChanged();
private:
    QString source_;
    QImage image_;
};
