#include "RasterImageItem.h"

#include <QPainter>

RasterImageItem::RasterImageItem(QQuickItem *parent) : QQuickPaintedItem(parent) {
    setAntialiasing(true);
}

void RasterImageItem::setSource(const QString &source) {
    if (source_ == source)
        return;
    source_ = source;
    image_ = {};
    if (source.startsWith(QStringLiteral("data:"))) {
        const qsizetype comma = source.indexOf(',');
        if (comma >= 0)
            image_.loadFromData(QByteArray::fromBase64(source.mid(comma + 1).toLatin1()));
    } else {
        QString path = source;
        if (path.startsWith(QStringLiteral("qrc:/")))
            path.replace(0, 4, QStringLiteral(":"));
        image_.load(path);
    }
    emit sourceChanged();
    update();
}

void RasterImageItem::paint(QPainter *painter) {
    if (image_.isNull() || width() <= 0 || height() <= 0)
        return;
    const QSize fitted = image_.size().scaled(qRound(width()), qRound(height()), Qt::KeepAspectRatio);
    const QRectF target((width() - fitted.width()) / 2, (height() - fitted.height()) / 2,
                        fitted.width(), fitted.height());
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    painter->drawImage(target, image_);
}
