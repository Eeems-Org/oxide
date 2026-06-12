/*!
 * \addtogroup QML
 * \brief The QML module
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <libblight/clock.h>
#include <libblight/types.h>
#include <mutex>

#include <QBrush>
#include <QImage>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickPaintedItem>
#include <QTimer>

namespace Oxide {
  /*!
   * \brief The Oxide::QML namespace
   */
  namespace QML {
    /*!
     * \brief An object available as Oxide in qml that provides useful
     * properties and methods
     *
     * Example:
     *
     * ```qml
     * Item {
     *   enabled: Oxide.landscape && Oxide.deviceName === "reMarkable 2"
     * }
     * ```
     */
    class LIBOXIDE_EXPORT OxideQml : public QObject {
      Q_OBJECT
      /*!
       * \brief If the device should be in landscape or not
       * \accessors landscape()
       * \notifier landscapeChanged(bool)
       */
      Q_PROPERTY(bool landscape READ landscape NOTIFY landscapeChanged)
      Q_PROPERTY(
        Blight::WaveformMode waveform READ waveform WRITE setWaveform MEMBER
          m_waveform NOTIFY waveformChanged
      )
      Q_PROPERTY(
        Blight::ContentType contentType READ contentType WRITE setContentType
          MEMBER m_contentType NOTIFY contentTypeChanged
      )
      Q_PROPERTY(
        Blight::UpdateMode updateMode READ updateMode WRITE setUpdateMode MEMBER
          m_updateMode NOTIFY updateModeChanged
      )
      /*!
       * \brief The name of the device
       * \accessors deviceName()
       */
      Q_PROPERTY(QString deviceName READ deviceName CONSTANT)
      QML_NAMED_ELEMENT(Oxide)
      QML_SINGLETON

    public:
      explicit OxideQml(QObject* parent = nullptr);
      /*!
       * \brief Check if the device should be in landscape mode or not
       * \return If the device should be in landscape or not
       * \sa landscape, landscapeChanged(bool)
       */
      bool landscape();
      /*!
       * \brief Get the current waveform mode
       * \return The current waveform mode
       * \sa waveform, setWaveform(Blight::WaveformMode waveform)
       * \sa waveformChanged(Blight::WaveformMode)
       */
      Blight::WaveformMode waveform();
      /*!
       * \brief Change the waveform mode
       * \param waveform The waveform to change it to
       * \sa waveform, waveformChanged(Blight::WaveformMode)
       */
      void setWaveform(Blight::WaveformMode waveform);
      /*!
       * \brief Get the current waveform mode
       * \return The current waveform mode
       * \sa waveform, setWaveform(Blight::WaveformMode waveform)
       * \sa waveformChanged(Blight::WaveformMode)
       */
      Blight::ContentType contentType();
      /*!
       * \brief Change the contentType mode
       * \param contentType The contentType to change it to
       * \sa contentType, contentTypeChanged(Blight::ContentType)
       */
      void setContentType(Blight::ContentType contentType);
      /*!
       * \brief Get the current updateMode mode
       * \return The current updateMode mode
       * \sa updateMode, setWaveform(Blight::UpdateMode updateMode)
       * \sa updateModeChanged(Blight::UpdateMode)
       */
      Blight::UpdateMode updateMode();
      /*!
       * \brief Change the updateMode mode
       * \param updateMode The updateMode to change it to
       * \sa updateMode, updateModeChanged(Blight::UpdateMode)
       */
      void setUpdateMode(Blight::UpdateMode updateMode);
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
      /*!
       * \brief The value of waveform changed
       * \sa waveform, waveform(), setWaveform(Blight::WaveformMode waveform)
       */
      void waveformChanged(Blight::WaveformMode);
      /*!
       * \brief The value of contentType changed
       * \sa contentType, contentType(), setContentType(Blight::ContentType
       * contentType)
       */
      void contentTypeChanged(Blight::ContentType);
      /*!
       * \brief The value of updateMode changed
       * \sa updateMode, updateMode(), setUpdateMode(Blight::UpdateMode
       * updateMode)
       */
      void updateModeChanged(Blight::UpdateMode);

    private:
      Blight::WaveformMode m_waveform;
      Blight::ContentType m_contentType;
      Blight::UpdateMode m_updateMode;
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
    class LIBOXIDE_EXPORT Canvas : public QQuickPaintedItem {
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
      Q_PROPERTY(
        qreal penWidth READ penWidth WRITE setPenWidth NOTIFY penWidthChanged
      )
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
      void geometryChange(
        const QRectF& newGeometry,
        const QRectF& oldGeometry
      ) override;
      void mousePressEvent(QMouseEvent* event) override;
      void mouseMoveEvent(QMouseEvent* event) override;
      void mouseReleaseEvent(QMouseEvent* event) override;

    private:
      QPointF m_lastPoint;
      QImage m_drawn;
      QBrush m_brush;
      qreal m_penWidth;
      QRegion m_repainted;
      QRegion m_pending;
      QTimer m_finalizeTimer;
      QTimer m_ghostControlTimer;
      Blight::ClockWatch m_LastPaint;
      std::mutex m_timerMutex;
      void applyPending();
    };
    /*!
     * \brief Get the display server buffer that represents a surface for a
     * QWindow
     * \param window The Window to get the surface buffer for
     * \return The buffer that represents the display server surface
     */
    LIBOXIDE_EXPORT Blight::shared_buf_t getSurfaceForWindow(QWindow* window);
    /*!
     * \brief Get the a QImage that can be used to manipulate a display
     * server buffer
     * \param buffer The buffer to create the QImage instance for
     * \return A QImage instance that can be used to manipulate a display
     * server buffer
     */
    LIBOXIDE_EXPORT QImage getImageForSurface(Blight::shared_buf_t buffer);
    /*!
     * \brief Get a QImage instance that can be used to manipulate a display
     * server buffer for a QWindow
     * \param window The QWindow instance
     * \return A QImage instance that can be used to manipulate a display
     * server buffer for the QWindow
     */
    LIBOXIDE_EXPORT QImage getImageForWindow(QWindow* window);
    /*!
     * \brief Repaint a surface for a QWindow on the display server
     * \param window The QWindow instance to repaint
     * \param rect The area of the QWindow to repaint
     * \param waveform The waveform to use for repainting
     * \param sync If the method should wait for the repaint to finish
     * before continuing
     */
    LIBOXIDE_EXPORT void repaint(
      QWindow* window,
      QRectF rect,
      Blight::WaveformMode waveform = Blight::WaveformMode::UI,
      Blight::ContentType contentType = Blight::ContentType::Color,
      Blight::UpdateMode updateMode = Blight::UpdateMode::PartialUpdate,
      bool sync = false
    );
    /*!
     * \brief Get the OxideQML singleton instance
     * \return The OxideQML singleton instance
     * \sa OxideQML
     */
    LIBOXIDE_EXPORT OxideQml* getSingleton();
    /*!
     * \brief Register the %QML extensions that liboxide provides
     * \param engine The QQmlApplicationEngine instance that this
     * application is using
     * \sa OxideQml, Canvas
     *
     * This registers the OxideQML singleton so that it can be accessed via
     * Oxide in QML. It also registers the built in QML classes that
     * liboxide provides so that they can be imported with `import
     * "qrc://codes.eeems.oxide"`. Other classes are available for import
     * with `import codes.eeems.oxide 3.0`
     */
    LIBOXIDE_EXPORT void registerQML(QQmlApplicationEngine* engine);
  } // namespace QML
} // namespace Oxide
/*! @} */
