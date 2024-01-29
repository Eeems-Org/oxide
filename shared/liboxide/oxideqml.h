#pragma once

#include <QObject>
#include <QImage>
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickPaintedItem>
#include <QBrush>

#ifdef EPAPER
#include <libblight/types.h>
#endif

namespace Oxide {
    namespace QML{

        class OxideQml : public QObject{
            Q_OBJECT
            Q_PROPERTY(bool landscape READ landscape NOTIFY landscapeChanged)
            QML_NAMED_ELEMENT(Oxide)
            QML_SINGLETON
        public:
            explicit OxideQml(QObject *parent = nullptr);
            bool landscape();
            Q_INVOKABLE QBrush brushFromColor(const QColor& color);

        signals:
            void landscapeChanged(bool);
        };

        class Canvas : public QQuickPaintedItem {
            Q_OBJECT
            Q_PROPERTY(QBrush brush READ brush WRITE setBrush NOTIFY brushChanged)
            Q_PROPERTY(qreal penWidth READ penWidth WRITE setPenWidth NOTIFY penWidthChanged)
            QML_ELEMENT
        public:
            Canvas(QQuickItem* parent = nullptr);
            void paint(QPainter* painter) override;
            QBrush brush();
            void setBrush(QBrush brush);
            qreal penWidth();
            void setPenWidth(qreal penWidth);
            QImage* image();

        signals:
            void brushChanged(const QBrush& brush);
            void penWidthChanged(qreal pendWidth);

        protected:
            void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry) override;
            void mousePressEvent(QMouseEvent *event) override;
            void mouseMoveEvent(QMouseEvent *event) override;
            void mouseReleaseEvent(QMouseEvent *event) override;

        private:
            QPointF m_lastPoint;
            QImage m_drawn;
            QBrush m_brush;
            qreal m_penWidth;
        };

#ifdef EPAPER
        Blight::shared_buf_t getSurfaceForWindow(QWindow* window);
#endif

        OxideQml* getSingleton();
        void registerQML(QQmlApplicationEngine* engine);
    }
}
