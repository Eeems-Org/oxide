#ifndef SCREENPROVIDER_H
#define SCREENPROVIDER_H

#include <QQuickImageProvider>

class ScreenProvider: public QObject, public QQuickImageProvider
{
    Q_OBJECT
public:
    ScreenProvider(QObject* parent) : QObject(parent), QQuickImageProvider(QQuickImageProvider::Image), image() {};

public slots:
    void updateImage(QImage* image){
        this->image = image->copy(image->rect());
        emit imageChanged();
    }
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override {
        Q_UNUSED(id);
        if(size){
            *size = image.size();
        }
        if(requestedSize.width() > 0 && requestedSize.height() > 0) {
            image = image.scaled(requestedSize.width(), requestedSize.height(), Qt::KeepAspectRatio);
        }
        return image;
    }

signals:
    void imageChanged();

private:
    QImage image;
};

#endif // SCREENPROVIDER_H
