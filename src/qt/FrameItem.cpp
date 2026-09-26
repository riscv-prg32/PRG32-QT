#include "FrameItem.h"
#include <QFontDatabase>
#include <QPainter>
#include <algorithm>
void FrameItem::setFrame(const std::vector<uint16_t>& p) {
    QImage im(320, 200, QImage::Format_RGB888);
    for (int y = 0; y < 200; y++) {
        auto* row = im.scanLine(y);
        for (int x = 0; x < 320; x++) {
            uint16_t c = p[y * 320 + x];
            row[x * 3 + 0] = uint8_t(((c >> 11) & 31) * 255 / 31);
            row[x * 3 + 1] = uint8_t(((c >> 5) & 63) * 255 / 63);
            row[x * 3 + 2] = uint8_t((c & 31) * 255 / 31);
        }
    }
    image_ = std::move(im);
    update();
}
void FrameItem::paint(QPainter* p) {
    p->setRenderHint(QPainter::SmoothPixmapTransform, false);
    p->fillRect(boundingRect(), Qt::black);
    if (!image_.isNull()) {
        QSizeF size(320, statusBarsEnabled_ ? 240 : 200);
        size.scale(boundingRect().size(), Qt::KeepAspectRatio);
        QRectF dest(QPointF((width() - size.width()) / 2, (height() - size.height()) / 2), size);
        const qreal scale = size.width() / 320.0;
        const qreal bandHeight = 20.0 * scale;
        QRectF gameRect = dest;
        if (statusBarsEnabled_) {
            gameRect.adjust(0, bandHeight, 0, -bandHeight);
            p->fillRect(QRectF(dest.left(), dest.top(), dest.width(), bandHeight), Qt::black);
            p->fillRect(QRectF(dest.left(), dest.bottom() - bandHeight, dest.width(), bandHeight), Qt::black);
            QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
            font.setPixelSize(std::max(1, int(8.0 * scale)));
            p->setFont(font);
            p->setPen(QColor(0, 255, 255));
            p->drawText(QRectF(dest.left() + 4.0 * scale, dest.top(), dest.width(), bandHeight),
                        Qt::AlignVCenter | Qt::AlignLeft,
                        QString("FPS:%1").arg(framesPerSecond_));
            p->setPen(QColor(0, 255, 0));
            p->drawText(QRectF(dest.left() + 4.0 * scale,
                               dest.bottom() - bandHeight,
                               dest.width() - 8.0 * scale,
                               bandHeight),
                        Qt::AlignVCenter | Qt::AlignLeft,
                        statusText_.isEmpty() ? QStringLiteral("PRG32") : statusText_);
        }
        p->drawImage(gameRect, image_);
    }
}
void FrameItem::setStatusBarsEnabled(bool enabled) {
    if (statusBarsEnabled_ == enabled)
        return;
    statusBarsEnabled_ = enabled;
    update();
    emit statusBarsEnabledChanged();
}
void FrameItem::setStatusText(const QString& text) {
    if (statusText_ == text)
        return;
    statusText_ = text;
    update();
    emit statusTextChanged();
}
void FrameItem::setFramesPerSecond(int framesPerSecond) {
    if (framesPerSecond_ == framesPerSecond)
        return;
    framesPerSecond_ = framesPerSecond;
    update();
    emit framesPerSecondChanged();
}
