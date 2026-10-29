/*!
 * \addtogroup QML
 * \brief The QML module
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <functional>
#include <libblight/clock.h>
#include <libblight/types.h>
#include <mutex>

#include <QBrush>
#include <QFileSystemWatcher>
#include <QImage>
#include <QObject>
#include <QPen>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickPaintedItem>
#include <QQuickWindow>
#include <QTabletEvent>
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
      /*!
       * \brief The height in pixels of the top system bar (topbar header)
       *
       * Set by the topbar on startup so that other OxideWindow instances can
       * reserve the correct amount of space below it. Backed by
       * SharedSettings so the value persists across processes.
       *
       * \accessors topSystemSpace(), setTopSystemSpace(int)
       * \notifier topSystemSpaceChanged(int)
       */
      Q_PROPERTY(
        int topSystemSpace READ topSystemSpace WRITE setTopSystemSpace NOTIFY
          topSystemSpaceChanged
      )
      /*!
       * \brief The height in pixels of the bottom system bar
       *
       * Backed by SharedSettings so the value persists across processes.
       *
       * \accessors bottomSystemSpace(), setBottomSystemSpace(int)
       * \notifier bottomSystemSpaceChanged(int)
       */
      Q_PROPERTY(
        int bottomSystemSpace READ bottomSystemSpace WRITE setBottomSystemSpace
          NOTIFY bottomSystemSpaceChanged
      )
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
       * \brief Get the top system bar height in pixels
       * \return The top system bar height in pixels
       * \sa topSystemSpace, setTopSystemSpace(int)
       * \sa topSystemSpaceChanged(int)
       */
      int topSystemSpace();
      /*!
       * \brief Set the top system bar height in pixels
       * \param height The top system bar height in pixels
       * \sa topSystemSpace(), topSystemSpaceChanged(int)
       */
      void setTopSystemSpace(int height);
      /*!
       * \brief Get the bottom system bar height in pixels
       * \return The bottom system bar height in pixels
       * \sa bottomSystemSpace, setBottomSystemSpace(int)
       * \sa bottomSystemSpaceChanged(int)
       */
      int bottomSystemSpace();
      /*!
       * \brief Set the bottom system bar height in pixels
       * \param height The bottom system bar height in pixels
       * \sa bottomSystemSpace(), bottomSystemSpaceChanged(int)
       */
      void setBottomSystemSpace(int height);
      /*!
       * \brief Get a QBrush for a colour
       * \param color Colour
       * \param style Style
       * \return QBrush for a colour
       */
      Q_INVOKABLE QBrush brushFromColor(
        const QColor& color,
        Qt::BrushStyle style = Qt::SolidPattern
      );
      /*!
       * \brief Create a QPen
       * \param brush QBrush instance to use
       * \param width Width of the pen
       * \param style Style of the pen
       * \param cap Cap style of the pen
       * \param join Join style of the pen
       * \return QPen
       */
      Q_INVOKABLE QPen createPen(
        const QBrush& brush,
        qreal width,
        Qt::PenStyle style = Qt::SolidLine,
        Qt::PenCapStyle cap = Qt::SquareCap,
        Qt::PenJoinStyle join = Qt::BevelJoin
      );

    signals:
      /*!
       * \brief The value of landscape changed
       * \sa landscape, landscape()
       */
      void landscapeChanged(bool);
      /*!
       * \brief The value of topSystemSpace changed
       * \sa topSystemSpace, topSystemSpace()
       */
      void topSystemSpaceChanged(int);
      /*!
       * \brief The value of bottomSystemSpace changed
       * \sa bottomSystemSpace, bottomSystemSpace()
       */
      void bottomSystemSpaceChanged(int);
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
     *  initialItem: OxideCanvas{
     *    anchors.fill: parent
     *  }
     * }
     * ```
     */
    class LIBOXIDE_EXPORT OxideCanvas : public QQuickPaintedItem {
      Q_OBJECT
      /*!
       * \property pen
       * \brief Current pen used when drawing
       * \accessors pen(), setPen(QPen)
       * \notifier penChanged(const QPen&)
       */
      Q_PROPERTY(QPen pen READ pen NOTIFY penChanged)
      QML_ELEMENT

    public:
      /*!
       * \brief Pen state enum
       */
      enum State { Inactive, Hovering, Drawing };
      Q_ENUM(State)

      /*!
       * \brief Create a new canvas instance
       * \param parent Parent widget
       */
      OxideCanvas(QQuickItem* parent = nullptr);
      ~OxideCanvas();
      void paint(QPainter* painter) override;
      /*!
       * \brief Current pen used when drawing
       * \return The current pen
       * \sa pen, setBrush(QPen), penChanged(const QPen&)
       */
      QPen pen();
      /*!
       * \brief Set the current pen that is used for drawing
       * \param pen Pen to use
       * \sa pen, pen(), penChanged(const QPen&)
       */
      Q_INVOKABLE void setPen(QPen pen);
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
       * \brief The current pen used for drawing has been changed
       * \param pen New pen
       * \sa pen, pen(), setPen(QPen)
       */
      void penChanged(const QPen& pen);

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
      QPen m_pen;
      QRegion m_pending;
      QTimer m_finalizeTimer;
      QTimer m_ghostControlTimer;
      Blight::ClockWatch m_LastPaint;
      std::atomic<bool> m_drawing;
      std::atomic<bool> m_hovering;
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
     * \sa OxideQml, OxideCanvas
     *
     * This registers the OxideQML singleton so that it can be accessed via
     * Oxide in QML. It also registers the built in QML classes that
     * liboxide provides so that they can be imported with `import
     * "qrc://codes.eeems.oxide"`. Other classes are available for import
     * with `import codes.eeems.oxide 3.0`
     */
    LIBOXIDE_EXPORT void registerQML(QQmlApplicationEngine* engine);
    /*!
     * \brief Load a QML file and return the created root object.
     * \param engine The QQmlApplicationEngine to load into
     * \param url The URL of the QML file to load
     * \return The created QObject, or nullptr on failure
     * \sa enableHotReload
     */
    LIBOXIDE_EXPORT QObject*
    loadQML(QQmlApplicationEngine* engine, const QUrl& url);
  } // namespace QML
} // namespace Oxide
/*! @} */
