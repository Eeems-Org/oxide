#include <QPainterPath>

#include "notificationapi.h"
#include "systemapi.h"

QRect NotificationAPI::paintNotification(const QString &text, const QString &iconPath){
    QImage notification = notificationImage(text, iconPath);
    return dispatchToMainThread<QRect>([&notification]{
        qDebug() << "Painting to framebuffer...";
        auto frameBuffer = EPFrameBuffer::framebuffer();
        QPainter painter(frameBuffer);
        QPoint pos(0, frameBuffer->height() - notification.height());
        if(systemAPI->landscape()){
            notification = notification.transformed(QTransform().rotate(90.0));
            pos.setX(0);
            pos.setY(frameBuffer->height() - notification.height());
        }
        auto updateRect = notification.rect().translated(pos);
        painter.drawImage(updateRect, notification);
        painter.end();
        qDebug() << "Updating screen " << updateRect << "...";
        EPFrameBuffer::sendUpdate(
            updateRect,
            EPFrameBuffer::Grayscale,
            EPFrameBuffer::PartialUpdate,
            true
        );
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
QImage NotificationAPI::notificationImage(const QString& text, const QString& iconPath){
    auto padding = 10;
    auto radius = 10;
    auto frameBuffer = EPFrameBuffer::framebuffer();
    auto size = frameBuffer->size();
    auto boundingRect = QPainter(frameBuffer).fontMetrics().boundingRect(
        QRect(0, 0, size.width() / 2, size.height() / 8),
        Qt::AlignCenter | Qt::TextWordWrap, text
    );
    QImage icon(iconPath);
    auto iconSize = icon.isNull() ? 0 : 50;
    auto width = boundingRect.width() + iconSize + (padding * 3);
    auto height = max(boundingRect.height(), iconSize) + (padding * 2);
    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(image.rect(), radius, radius);
    QPen pen(Qt::white, 1);
    painter.setPen(pen);
    painter.fillPath(path, Qt::black);
    painter.drawPath(path);
    painter.setPen(Qt::white);
    QRect textRect(
        padding,
        padding,
        image.width() - iconSize - (padding * 2),
        image.height() - padding
    );
    painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, text);
    if(!icon.isNull()){
        QRect iconRect(
            image.width() - iconSize - padding,
            padding,
            iconSize,
            iconSize
        );
        painter.fillRect(iconRect, Qt::white);
        painter.drawImage(iconRect, icon);
    }
    painter.end();
    return image;
}

void NotificationAPI::drawNotificationText(const QString& text, QColor color, QColor background){
    auto frameBuffer = EPFrameBuffer::framebuffer();
    dispatchToMainThread([frameBuffer, text, color, background]{
        QPainter painter(frameBuffer);
        int padding = 10;
        auto size = frameBuffer->size();
        auto fm = painter.fontMetrics();
        int textHeight = fm.height() + padding;
        QImage textImage(
            size.width() - padding * 2,
            textHeight,
            background == Qt::transparent
                ? QImage::Format_ARGB32_Premultiplied
                : QImage::Format_RGB16
        );
        textImage.fill(Qt::transparent);
        QPainter painter2(&textImage);
        painter2.setPen(color);
        painter2.drawText(
            textImage.rect(),
            Qt::AlignVCenter | Qt::AlignRight,
            text
        );
        painter2.end();
        QPoint textPos(0 + padding, size.height() - textHeight);
        if(systemAPI->landscape()){
            textImage = textImage.transformed(QTransform().rotate(90.0));
            textPos.setX(0);
            textPos.setY(size.height() - textImage.height() - padding);
        }
        auto textRect = textImage.rect().translated(textPos);
        painter.drawImage(textRect, textImage);
        EPFrameBuffer::sendUpdate(
            textRect,
            EPFrameBuffer::Grayscale,
            EPFrameBuffer::PartialUpdate,
            true
        );
        painter.end();
    });
}
