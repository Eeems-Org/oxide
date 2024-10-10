#include "surfacewidget.h"
#include "dbusinterface.h"
#include "guithread.h"

#include <QQuickWindow>
#include <QSGImageNode>

#ifndef EPAPER
SurfaceWidget::SurfaceWidget(QQuickItem* parent)
: QQuickItem(parent)
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

std::shared_ptr<Surface> SurfaceWidget::surface(){
    return dbusInterface->getSurface(identifier());
}

std::shared_ptr<QImage> SurfaceWidget::image(){
    auto surface = this->surface();
    if(surface == nullptr){
        return nullptr;
    }
    return surface->image();
}
#endif
