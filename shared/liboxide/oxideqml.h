/*!
 * \addtogroup QML
 * \brief The QML module
 * @{
 * \file
 */
#pragma once

#include <QObject>
#include <QImage>
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickPaintedItem>
#include <QBrush>

#include <libblight/types.h>

namespace Oxide {
    /*!
     * \brief The Oxide::QML namespace
     */
    namespace QML{
        /*!
         * \brief An object available as Oxide in qml that provides useful properties and methods
         *
         * Example:
         *
         * ```qml
         * Item {
         *   enabled: Oxide.landscape && Oxide.deviceName === "reMarkable 2"
         * }
         * ```
         */
        class OxideQml : public QObject{
            Q_OBJECT
            /*!
             * \brief If the device should be in landscape or not
             * \accessors landscape()
             * \notifier landscapeChanged(bool)
             */
            Q_PROPERTY(bool landscape READ landscape NOTIFY landscapeChanged)
            /*!
             * \brief The name of the device
             * \accessors deviceName()
             */
            Q_PROPERTY(QString deviceName READ deviceName CONSTANT)
            QML_NAMED_ELEMENT(Oxide)
            QML_SINGLETON

        public:
            explicit OxideQml(QObject *parent = nullptr);
            /*!
             * \brief Check if the device should be in landscape mode or not
             * \return If the device should be in landscape or not
             * \sa landscape, landscapeChanged(bool)
             */
            bool landscape();
            /*!
             * \brief Get the name of the device
             * \return The name of the device
             * \retval "reMarkable 1"
             * \retval "reMarkable 2"
             * \retval "Unknown"
             * \sa deviceName
             */
            QString deviceName();
            /*!
             * \brief Get a QBrush for a colour
             * \param color Colour
             * \return QBrush for a colour
             */
            Q_INVOKABLE QBrush brushFromColor(const QColor& color);

        signals:
            /*!
             * \brief The value of landscape changed
             * \sa landscape, landscape()
             */
            void landscapeChanged(bool);
        };
        /*!
         * \brief A canvas widget
         *
         * Example:
         * ```qml
         * import "qrc://codes.eeems.oxide"
         * import codes.eeems.oxide 3.0
         *
         * OxideWindow{
         *  initialItem: Canvas{
         *    anchors.fill: parent
         *    brush: Oxide.brushFromColor(Qt.black)
         *    penWidth: 12
         *  }
         * }
         * ```
         */
        class Canvas : public QQuickPaintedItem {
            Q_OBJECT
            /*!
             * \property brush
             * \brief Current brush used when drawing
             * \accessors brush(), setBrush(QBrush)
             * \notifier brushChanged(const QBrush&)
             */
            Q_PROPERTY(QBrush brush READ brush WRITE setBrush NOTIFY brushChanged)
            /*!
             * \property penWidth
             * \brief Current pen width used when drawing
             * \accessors penWidth(), setPenWidth(qreal)
             * \notifier penWidthChanged(qreal)
             */
            Q_PROPERTY(qreal penWidth READ penWidth WRITE setPenWidth NOTIFY penWidthChanged)
            QML_ELEMENT

        public:
            /*!
             * \brief Create a new canvas instance
             * \param parent Parent widget
             */
            Canvas(QQuickItem* parent = nullptr);
            void paint(QPainter* painter) override;
            /*!
             * \brief Current brush used when drawing
             * \return The current brush
             * \sa brush, setBrush(QBrush), brushChanged(const QBrush&)
             */
            QBrush brush();
            /*!
             * \brief Set the current brush that is used for drawing
             * \param brush Brush to use
             * \sa brush, brush(), brushChanged(const QBrush&)
             */
            void setBrush(QBrush brush);
            /*!
             * \brief Current pen width used when drawing
             * \return The current pen with
             * \sa penWidth, setPenWidth(QBrush), penWidthChanged(const QBrush&)
             */
            qreal penWidth();
            /*!
             * \brief Set the current pen width used for drawing
             * \param penWidth Pen width to use
             * \sa penWidth, penWidth(), penWidthChanged(qreal)
             */
            void setPenWidth(qreal penWidth);
            /*!
             * \brief QImage instance of the current canvas
             * \return QImage instanceof the current canvas
             */
            QImage* image();

        signals:
            /*!
             * \brief The user has started drawing on the canvas
             */
            void drawStart();
            /*!
             * \brief The user has finished drawing on the canvas
             */
            void drawDone();
            /*!
             * \brief The current brush used for drawing has been changed
             * \param brush New brush
             * \sa brush, brush(), setBrush(QBrush)
             */
            void brushChanged(const QBrush& brush);
            /*!
             * \brief The current pen width used for drawing has been changed
             * \param pendWidth New pen width
             * \sa penWidth, penWidth(), setPenWidth(qreal)
             */
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
        /*!
         * \brief Get the display server buffer that represents a surface for a QWindow
         * \param window The Window to get the surface buffer for
         * \return The buffer that represents the display server surface
         */
        Blight::shared_buf_t getSurfaceForWindow(QWindow* window);
        /*!
         * \brief Get the a QImage that can be used to manipulate a display server buffer
         * \param buffer The buffer to create the QImage instance for
         * \return A QImage instance that can be used to manipulate a display server buffer
         */
        QImage getImageForSurface(Blight::shared_buf_t buffer);
        /*!
         * \brief Get a QImage instance that can be used to manipulate a display server buffer for a QWindow
         * \param window The QWindow instance
         * \return A QImage instance that can be used to manipulate a display server buffer for the QWindow
         */
        QImage getImageForWindow(QWindow* window);
        /*!
         * \brief Repaint a surface for a QWindow on the display server
         * \param window The QWindow instance to repaint
         * \param rect The area of the QWindow to repaint
         * \param waveform The waveform to use for repainting
         * \param sync If the method should wait for the repaint to finish before continuing
         */
        void repaint(
            QWindow* window,
            QRectF rect,
            Blight::WaveformMode waveform = Blight::WaveformMode::HighQualityGrayscale,
            bool sync = false
        );
        /*!
         * \brief Get the OxideQML singleton instance
         * \return The OxideQML singleton instance
         * \sa OxideQML
         */
        OxideQml* getSingleton();
        /*!
         * \brief Register the %QML extensions that liboxide provides
         * \param engine The QQmlApplicationEngine instance that this application is using
         * \sa OxideQml, Canvas
         *
         * This registers the OxideQML singleton so that it can be accessed via Oxide in QML.
         * It also registers the built in QML classes that liboxide provides so that they can be imported with `import "qrc://codes.eeems.oxide"`.
         * Other classes are available for import with `import codes.eeems.oxide 3.0`
         */
        void registerQML(QQmlApplicationEngine* engine);
    }
}
/*! @} */
