#include "surfacewidget.h"
#include "dbusinterface.h"

#include <QPainter>

SurfaceWidget::SurfaceWidget(QQuickItem* parent) : QQuickPaintedItem(parent){

}

QString SurfaceWidget::identifier(){ return m_identifier; }

void SurfaceWidget::setIdentifier(QString identifier){
    m_identifier = identifier;
    emit identifierChanged(m_identifier);
    updated();
}

void SurfaceWidget::updated(){ update(boundingRect().toRect()); }

void SurfaceWidget::paint(QPainter* painter){
    auto image = this->image();
    if(image != nullptr){
        painter->drawImage(boundingRect(), *image, image->rect());
    }
}

Surface* SurfaceWidget::surface(){ return DbusInterface::singleton->getSurface(identifier()); }

QImage* SurfaceWidget::image(){
    auto surface = this->surface();
    if(surface == nullptr){
        return nullptr;
    }
    return surface->image();
}
