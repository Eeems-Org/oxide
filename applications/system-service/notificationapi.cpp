#include "notificationapi.h"
#include "systemapi.h"

QRect NotificationAPI::paintNotification(const QString &text, const QString &iconPath){
    return dispatchToMainThread<QRect>([text, iconPath]{
        qDebug() << "Painting to framebuffer...";
        auto frameBuffer = EPFrameBuffer::framebuffer();
        QPainter painter(frameBuffer);
        auto size = frameBuffer->size();
        auto padding = 10;
        auto radius = 10;
        QImage icon(iconPath);
        auto iconSize = icon.isNull() ? 0 : 50;
        auto boundingRect = painter.fontMetrics().boundingRect(
            QRect(0, 0, size.width() / 2, size.height() / 8),
            Qt::AlignCenter | Qt::TextWordWrap, text);
        auto width = boundingRect.width() + iconSize + (padding * 3);
        auto height = max(boundingRect.height(), iconSize) + (padding * 2);
        auto left = size.width() - width;
        auto top = size.height() - height;
        QRect updateRect(left, top, width, height);
        painter.fillRect(updateRect, Qt::black);
        painter.setPen(Qt::black);
        painter.drawRoundedRect(updateRect, radius, radius);
        painter.setPen(Qt::white);
        QRect textRect(
            left + padding,
            top + padding,
            width - iconSize - (padding * 2),
            height - padding
        );
        painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, text);
        painter.end();
        qDebug() << "Updating screen " << updateRect << "...";
        EPFrameBuffer::sendUpdate(
            updateRect,
            EPFrameBuffer::Mono,
            EPFrameBuffer::PartialUpdate,
            true
        );
        if (!icon.isNull()) {
            QPainter painter2(frameBuffer);
            QRect iconRect(
                size.width() - iconSize - padding,
                top + padding,
                iconSize, iconSize
            );
            painter2.fillRect(iconRect, Qt::white);
            painter2.drawImage(iconRect, icon);
            painter2.end();
            EPFrameBuffer::sendUpdate(
                iconRect,
                EPFrameBuffer::Mono,
                EPFrameBuffer::PartialUpdate,
                true
            );
        }
        EPFrameBuffer::waitForLastUpdate();
        return updateRect;
    });
}
void NotificationAPI::errorNotification(const QString &text) {
    dispatchToMainThread([] {
        auto frameBuffer = EPFrameBuffer::framebuffer();
        qDebug() << "Waiting for other painting to finish...";
        while (frameBuffer->paintingActive()) {
            EPFrameBuffer::waitForLastUpdate();
        }
        qDebug() << "Displaying error text";
        QPainter painter(frameBuffer);
        painter.fillRect(frameBuffer->rect(), Qt::white);
        painter.end();
        EPFrameBuffer::sendUpdate(
            frameBuffer->rect(),
            EPFrameBuffer::Mono,
            EPFrameBuffer::FullUpdate,
            true
        );
    });
    notificationAPI->paintNotification(text, "");
}
