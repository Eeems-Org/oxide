/*
 * Large portions of this file were copied from https://github.com/qt/qtbase/blob/5.15/src/platformsupport/input/evdevtouch/qevdevtouchhandler.cpp
 * As such, this file is under LGPL instead of MIT
 */
#include "oxidetouchscreendata.h"

#include <QLoggingCategory>
#include <QDebug>
#include <QGuiApplication>
#include <private/qhighdpiscaling_p.h>
#include <liboxide.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(qLcEvdevTouch, "qt.qpa.input")
Q_LOGGING_CATEGORY(qLcEvents, "qt.qpa.input.events")

OxideTouchScreenData::OxideTouchScreenData(const QStringList& args)
: m_lastEventType(-1),
  m_currentSlot(0),
  m_timeStamp(0),
  m_lastTimeStamp(0),
  hw_range_x_min(0),
  hw_range_x_max(0),
  hw_range_y_min(0),
  hw_range_y_max(0),
  hw_pressure_min(0),
  hw_pressure_max(0),
  m_forceToActiveWindow(false),
  m_typeB(false),
  m_singleTouch(false)
{
    for(const QString& arg : args){
        if(arg == QStringLiteral("force_window")){
            m_forceToActiveWindow = true;
        }
    }
    m_singleTouch = !deviceSettings.supportsMultiTouch();
    m_typeB = deviceSettings.isTouchTypeB();
    hw_range_x_max = deviceSettings.getTouchWidth();
    hw_range_y_max = deviceSettings.getTouchHeight();
    hw_pressure_max = deviceSettings.getTouchPressure();
    hw_name = "oxide";
    int rotation = 0;
    bool invertx = false;
    bool inverty = false;
    for(auto item : QString::fromLatin1(deviceSettings.getTouchEnvSetting()).split(":")){
        auto spec = item.split("=");
        auto key = spec.first();
        if(key == "invertx"){
            invertx = true;
            continue;
        }
        if(key == "inverty"){
            inverty = true;
            continue;
        }
        if(key != "rotate" || spec.count() != 2){
            continue;
        }
        QString value = spec.last();
        bool ok;
        uint angle = value.toUInt(&ok);
        if(!ok){
            return;
        }
        switch(angle){
            case 90:
            case 180:
            case 270:
                rotation = angle;
            default:
            break;
        }
    }
    if(rotation){
        m_rotate = QTransform::fromTranslate(0.5, 0.5).rotate(rotation).translate(-0.5, -0.5);
    }
    if(invertx){
        m_rotate *= QTransform::fromTranslate(0.5, 0.5).scale(-1.0, 1.0).translate(-0.5, -0.5);
    }
    if(inverty){
        m_rotate *= QTransform::fromTranslate(0.5, 0.5).scale(1.0, -1.0).translate(-0.5, -0.5);
    }
    m_device = new QTouchDevice();
    m_device->setName("oxide");
    m_device->setType(QTouchDevice::TouchScreen);
    m_device->setCapabilities(QTouchDevice::Position | QTouchDevice::Area | QTouchDevice::Pressure);
    if(hw_pressure_max){
        m_device->setCapabilities(m_device->capabilities() | QTouchDevice::Pressure);
    }
    QWindowSystemInterface::registerTouchDevice(m_device);
}

void OxideTouchScreenData::addTouchPoint(const Contact& contact, Qt::TouchPointStates* combinedStates){
    QWindowSystemInterface::TouchPoint tp;
    tp.id = contact.trackingId;
    tp.flags = contact.flags;
    tp.state = contact.state;
    *combinedStates |= tp.state;

    // Store the HW coordinates for now, will be updated later.
    tp.area = QRectF(0, 0, contact.maj, contact.maj);
    tp.area.moveCenter(QPoint(contact.x, contact.y));
    tp.pressure = contact.pressure;

    // Get a normalized position in range 0..1.
    tp.normalPosition = QPointF((contact.x - hw_range_x_min) / qreal(hw_range_x_max - hw_range_x_min),
                                (contact.y - hw_range_y_min) / qreal(hw_range_y_max - hw_range_y_min));

    if(!m_rotate.isIdentity()){
        tp.normalPosition = m_rotate.map(tp.normalPosition);
    }
    tp.rawPositions.append(QPointF(contact.x, contact.y));
    m_touchPoints.append(tp);
}

void OxideTouchScreenData::processInputEvent(input_event* data){
    if(data->type == EV_ABS){
        if(data->code == ABS_MT_POSITION_X || (m_singleTouch && data->code == ABS_X)){
            m_currentData.x = qBound(hw_range_x_min, data->value, hw_range_x_max);
            if(m_singleTouch){
                m_contacts[m_currentSlot].x = m_currentData.x;
            }
            if(m_typeB){
                m_contacts[m_currentSlot].x = m_currentData.x;
                if(m_contacts[m_currentSlot].state == Qt::TouchPointStationary){
                    m_contacts[m_currentSlot].state = Qt::TouchPointMoved;
                }
            }
        }else if(data->code == ABS_MT_POSITION_Y || (m_singleTouch && data->code == ABS_Y)){
            m_currentData.y = qBound(hw_range_y_min, data->value, hw_range_y_max);
            if(m_singleTouch){
                m_contacts[m_currentSlot].y = m_currentData.y;
            }
            if(m_typeB){
                m_contacts[m_currentSlot].y = m_currentData.y;
                if(m_contacts[m_currentSlot].state == Qt::TouchPointStationary){
                    m_contacts[m_currentSlot].state = Qt::TouchPointMoved;
                }
            }
        }else if(data->code == ABS_MT_TRACKING_ID){
            m_currentData.trackingId = data->value;
            if(m_typeB){
                if(m_currentData.trackingId == -1){
                    m_contacts[m_currentSlot].state = Qt::TouchPointReleased;
                }else{
                    m_contacts[m_currentSlot].state = Qt::TouchPointPressed;
                    m_contacts[m_currentSlot].trackingId = m_currentData.trackingId;
                }
            }
        }else if(data->code == ABS_MT_TOUCH_MAJOR){
            m_currentData.maj = data->value;
            if(data->value == 0){
                m_currentData.state = Qt::TouchPointReleased;
            }
            if(m_typeB){
                m_contacts[m_currentSlot].maj = m_currentData.maj;
            }
        }else if(data->code == ABS_PRESSURE || data->code == ABS_MT_PRESSURE){
            if(Q_UNLIKELY(qLcEvents().isDebugEnabled()))
                qCDebug(qLcEvents, "EV_ABS code 0x%x: pressure %d; bounding to [%d,%d]",
                        data->code, data->value, hw_pressure_min, hw_pressure_max);
            m_currentData.pressure = qBound(hw_pressure_min, data->value, hw_pressure_max);
            if(m_typeB || m_singleTouch){
                m_contacts[m_currentSlot].pressure = m_currentData.pressure;
            }
        }else if(data->code == ABS_MT_SLOT){
            m_currentSlot = data->value;
        }
    }else if(data->type == EV_KEY && !m_typeB){
        if(data->code == BTN_TOUCH && data->value == 0){
            m_contacts[m_currentSlot].state = Qt::TouchPointReleased;
        }
    }else if(data->type == EV_SYN && data->code == SYN_MT_REPORT && m_lastEventType != EV_SYN){
        // If there is no tracking id, one will be generated later.
        // Until that use a temporary key.
        int key = m_currentData.trackingId;
        if(key == -1){
            key = m_contacts.count();
        }
        m_contacts.insert(key, m_currentData);
        m_currentData = Contact();
    }else if(data->type == EV_SYN && data->code == SYN_REPORT){
        // Ensure valid IDs even when the driver does not report ABS_MT_TRACKING_ID.
        if(!m_contacts.isEmpty() && m_contacts.constBegin().value().trackingId == -1){
            assignIds();
        }
        // update timestamps
        m_lastTimeStamp = m_timeStamp;
        m_timeStamp = data->time.tv_sec + data->time.tv_usec / 1000000.0;

        m_lastTouchPoints = m_touchPoints;
        m_touchPoints.clear();
        Qt::TouchPointStates combinedStates;
        bool hasPressure = false;

        for(auto i = m_contacts.begin(), end = m_contacts.end(); i != end; /*erasing*/){
            auto it = i++;
            Contact& contact(it.value());
            if(!contact.state){
                continue;
            }
            int key = m_typeB ? it.key() : contact.trackingId;
            if(!m_typeB && m_lastContacts.contains(key)){
                const Contact& prev(m_lastContacts.value(key));
                if(contact.state == Qt::TouchPointReleased){
                    // Copy over the previous values for released points, just in case.
                    contact.x = prev.x;
                    contact.y = prev.y;
                    contact.maj = prev.maj;
                }else{
                    contact.state = (prev.x == contact.x && prev.y == contact.y) ? Qt::TouchPointStationary : Qt::TouchPointMoved;
                }
            }

            // Avoid reporting a contact in released state more than once.
            if(!m_typeB && contact.state == Qt::TouchPointReleased && !m_lastContacts.contains(key)){
                m_contacts.erase(it);
                continue;
            }
            if(contact.pressure){
                hasPressure = true;
            }
            addTouchPoint(contact, &combinedStates);
        }

        // Now look for contacts that have disappeared since the last sync.
        for(auto it = m_lastContacts.begin(), end = m_lastContacts.end(); it != end; ++it){
            Contact& contact(it.value());
            int key = m_typeB ? it.key() : contact.trackingId;
            if(m_typeB){
                if(contact.trackingId != m_contacts[key].trackingId && contact.state){
                    contact.state = Qt::TouchPointReleased;
                    addTouchPoint(contact, &combinedStates);
                }
            }else{
                if(!m_contacts.contains(key)){
                    contact.state = Qt::TouchPointReleased;
                    addTouchPoint(contact, &combinedStates);
                }
            }
        }

        // Remove contacts that have just been reported as released.
        for(auto i = m_contacts.begin(), end = m_contacts.end(); i != end; /*erasing*/){
            auto it = i++;
            Contact& contact(it.value());
            if(!contact.state){
                continue;
            }
            if(contact.state == Qt::TouchPointReleased){
                if(m_typeB){
                    contact.state = static_cast<Qt::TouchPointState>(0);
                }else{
                    m_contacts.erase(it);
                }
            }else{
                contact.state = Qt::TouchPointStationary;
            }
        }
        m_lastContacts = m_contacts;
        if(!m_typeB && !m_singleTouch){
            m_contacts.clear();
        }
        if(!m_touchPoints.isEmpty() && (hasPressure || combinedStates != Qt::TouchPointStationary)){
            reportPoints();
        }
    }
    m_lastEventType = data->type;
}

int OxideTouchScreenData::findClosestContact(const QHash<int, Contact>& contacts, int x, int y, int* dist){
    int minDist = -1, id = -1;
    for(QHash<int, Contact>::const_iterator it = contacts.constBegin(), ite = contacts.constEnd(); it != ite; ++it){
        const Contact &contact(it.value());
        int dx = x - contact.x;
        int dy = y - contact.y;
        int dist = dx * dx + dy * dy;
        if(minDist == -1 || dist < minDist){
            minDist = dist;
            id = contact.trackingId;
        }
    }
    if(dist){
        *dist = minDist;
    }
    return id;
}

void OxideTouchScreenData::assignIds(){
    QHash<int, Contact> candidates = m_lastContacts, pending = m_contacts, newContacts;
    int maxId = -1;
    QHash<int, Contact>::iterator it, ite, bestMatch;
    while(!pending.isEmpty() && !candidates.isEmpty()){
        int bestDist = -1, bestId = 0;
        for(it = pending.begin(), ite = pending.end(); it != ite; ++it){
            int dist;
            int id = findClosestContact(candidates, it->x, it->y, &dist);
            if(id >= 0 && (bestDist == -1 || dist < bestDist)){
                bestDist = dist;
                bestId = id;
                bestMatch = it;
            }
        }
        if(bestDist >= 0){
            bestMatch->trackingId = bestId;
            newContacts.insert(bestId, *bestMatch);
            candidates.remove(bestId);
            pending.erase(bestMatch);
            if(bestId > maxId){
                maxId = bestId;
            }
        }
    }
    if(candidates.isEmpty()){
        for(it = pending.begin(), ite = pending.end(); it != ite; ++it){
            it->trackingId = ++maxId;
            newContacts.insert(it->trackingId, *it);
        }
    }
    m_contacts = newContacts;
}

QRect OxideTouchScreenData::screenGeometry() const{
    if(m_forceToActiveWindow){
        QWindow* win = QGuiApplication::focusWindow();
        return win ? QHighDpi::toNativePixels(win->geometry(), win) : QRect();
    }
    QScreen* screen = QGuiApplication::primaryScreen();
    return QHighDpi::toNativePixels(screen->geometry(), screen);
}

void OxideTouchScreenData::reportPoints(){
    QRect winRect = screenGeometry();
    if(winRect.isNull()){
        qDebug() << __PRETTY_FUNCTION__ << "Null screenGeometry";
        return;
    }

    const int hw_w = hw_range_x_max - hw_range_x_min;
    const int hw_h = hw_range_y_max - hw_range_y_min;

    // Map the coordinates based on the normalized position. QPA expects 'area'
    // to be in screen coordinates.
    const int pointCount = m_touchPoints.count();
    for(int i = 0; i < pointCount; ++i){
        QWindowSystemInterface::TouchPoint &tp(m_touchPoints[i]);

        // Generate a screen position that is always inside the active window
        // or the primary screen.  Even though we report this as a QRectF, internally
        // Qt uses QRect/QPoint so we need to bound the size to winRect.size() - QSize(1, 1)
        const qreal wx = winRect.left() + tp.normalPosition.x() * (winRect.width() - 1);
        const qreal wy = winRect.top() + tp.normalPosition.y() * (winRect.height() - 1);
        const qreal sizeRatio = (winRect.width() + winRect.height()) / qreal(hw_w + hw_h);
        if(tp.area.width() == -1){
            // touch major was not provided
            tp.area = QRectF(0, 0, 8, 8);
        }else{
            tp.area = QRectF(0, 0, tp.area.width() * sizeRatio, tp.area.height() * sizeRatio);
        }
        tp.area.moveCenter(QPointF(wx, wy));

        // Calculate normalized pressure.
        if(!hw_pressure_min && !hw_pressure_max){
            tp.pressure = tp.state == Qt::TouchPointReleased ? 0 : 1;
        }else{
            tp.pressure = (tp.pressure - hw_pressure_min) / qreal(hw_pressure_max - hw_pressure_min);
        }

        if(Q_UNLIKELY(qLcEvents().isDebugEnabled())){
            qCDebug(qLcEvents) << "reporting" << tp;
        }
    }
    QWindowSystemInterface::handleTouchEvent(nullptr, m_device, m_touchPoints);
}

QT_END_NAMESPACE
