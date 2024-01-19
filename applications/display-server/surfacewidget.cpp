#include "surfacewidget.h"
#include "dbusinterface.h"

#include <QQuickWindow>

#ifdef EPAPER
#include <QPainter>
#else
#include <QSGImageNode>
#endif

SurfaceWidget::SurfaceWidget(QQuickItem* parent)
#ifdef EPAPER
: QQuickPaintedItem(parent)
#else
: QQuickItem(parent)
#endif
{
    setObjectName("Surface");
    setFlag(QQuickItem::ItemHasContents);
}

QString SurfaceWidget::identifier(){ return m_identifier; }

void SurfaceWidget::setIdentifier(QString identifier){
    m_identifier = identifier;
    emit identifierChanged(m_identifier);
    updated();
}

void SurfaceWidget::updated(){ update(); }

#ifdef EPAPER
void SurfaceWidget::paint(QPainter* painter){
    auto image = this->image();
    if(image == nullptr || image->isNull()){
        return;
    }
    painter->drawImage(painter->clipBoundingRect(), *image, painter->clipBoundingRect());
}
#else
QSGNode* SurfaceWidget::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*){
    if(!oldNode){
        oldNode = window()->createImageNode();
    }
    QRectF rect = boundingRect();
    if (rect.isEmpty()) {
        return oldNode;
    }
    auto image = this->image();
    if(image == nullptr || image->isNull()){
        return oldNode;
    }
    QSGTexture* texture = window()->createTextureFromImage(*image);
    if(!texture){
        return oldNode;
    }
    auto node = static_cast<QSGImageNode*>(oldNode);
    node->setTexture(texture);
    node->setRect(rect);
    return oldNode;
}
#endif

Surface* SurfaceWidget::surface(){ return DbusInterface::singleton->getSurface(identifier()); }

QImage* SurfaceWidget::image(){
    auto surface = this->surface();
    if(surface == nullptr){
        return nullptr;
    }
    return surface->image();
}
