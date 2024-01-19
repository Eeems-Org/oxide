#pragma once
#include <QSGImageNode>
#include <QPainter>
#include <QRectF>

class EPImageNode : QSGImageNode {
public:
     EPImageNode();
    ~EPImageNode();

    void draw(QPainter* painter);
    void setFiltering(QSGTexture::Filtering filtering);
    static void setHorizontalWrapMode(QSGTexture::WrapMode wrapMode);
    static void setInnerSourceRect(QRectF* rect);
    static void setInnerTargetRect(QRectF* rect);
    static bool setMirror(bool mirror);
    void setSubSourceRect(QRectF* rect);
    static void setTargetRect(QRectF* rect);
    void setTexture(QSGTexture* texture);

public slots:
    void update();
    void updateCached();
};
