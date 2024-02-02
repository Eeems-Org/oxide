#include "oxideeventhandler.h"
#include "oxideeventmanager.h"
#include "default_keymap.h"

#include <private/qhighdpiscaling_p.h>
#include <libblight.h>
#include <libblight/socket.h>
#include <liboxide/debug.h>
#include <liboxide/devicesettings.h>
#include <private/qevdevtouchfilter_p.h>

typedef struct KeyboardData {
    quint16 m_modifiers;
    quint8 m_locks[3];
    int m_composing;
    quint16 m_dead_unicode;
} KeyboardData;

typedef struct rect_t {
    int x, y, p, d;
} rect_t;

typedef struct TabletData {
    rect_t minValues;
    rect_t maxValues;
    struct {
        int x, y, p, d;
        bool down, lastReportDown;
        int tool, lastReportTool;
        QPointF lastReportPos;
    } state;
    int lastEventType;

    TabletData()
    : lastEventType{0}
    {
        memset(&minValues, 0, sizeof(minValues));
        memset(&maxValues, 0, sizeof(maxValues));
        memset(&maxValues, 0, sizeof(maxValues));
        memset((void*)&state, 0, (sizeof(int) * 6) + (sizeof(bool) * 2));
    }
} TabletData;

typedef struct Contact {
    int trackingId = -1;
    int x = 0;
    int y = 0;
    int maj = -1;
    int pressure = 0;
    Qt::TouchPointState state = Qt::TouchPointPressed;
    QTouchEvent::TouchPoint::InfoFlags flags;
} Contact;

typedef struct TouchData {
    int lastEventType;
    Contact currentData;
    int currentSlot;
    double timeStamp;
    double lastTimeStamp;
    int minX;
    int maxX;
    int minY;
    int maxY;
    int minPressure;
    int maxPressure;
    bool isTypeB;
    bool singleTouch;
    QList<QWindowSystemInterface::TouchPoint> touchPoints;
    QList<QWindowSystemInterface::TouchPoint> lastTouchPoints;
    QHash<int, Contact> contacts;
    QHash<int, Contact> lastContacts;
    QTouchDevice* device = nullptr;
    TouchData()
    : lastEventType{-1},
      currentSlot{0},
      timeStamp{0},
      lastTimeStamp{0},
      minX{0},
      maxX{0},
      minY{0},
      maxY{0},
      minPressure{0},
      maxPressure{0},
      isTypeB{false},
      singleTouch{false},
      touchPoints{},
      lastTouchPoints{},
      contacts{},
      lastContacts{}
    {}
    ~TouchData(){
        if(device != nullptr){
            QWindowSystemInterface::unregisterTouchDevice(device);
            delete device;
        }
    }
} TouchData;

typedef struct PointerData {
    // TODO - populate
} PointerData;

DeviceData::DeviceData() : DeviceData(0, QInputDeviceManager::DeviceTypeUnknown){}

#define LONG_BITS (sizeof(long) << 3)
#define NUM_LONGS(bits) (((bits) + LONG_BITS - 1) / LONG_BITS)
static inline bool testBit(long bit, const long *array){
    return (array[bit / LONG_BITS] >> bit % LONG_BITS) & 1;
}

DeviceData::DeviceData(unsigned int device, QInputDeviceManager::DeviceType type)
: device(device),
  type(type)
{
    auto path = QString("/dev/input/event%1").arg(device);
    auto fd = ::open(path.toStdString().c_str(), O_RDWR);
    char name[1024];
    QString hw_name;
    if(ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0){
        hw_name = QString::fromLocal8Bit(name);
    }
    switch(type){
        case QInputDeviceManager::DeviceTypeKeyboard:{
            auto keyboardData = set<KeyboardData>();
            keyboardData->m_dead_unicode = 0xffff;
            break;
        }
        case QInputDeviceManager::DeviceTypeTablet:{
            auto tabletData = new TabletData();
            m_data = tabletData;
            input_absinfo absInfo;
            memset(&absInfo, 0, sizeof(input_absinfo));
            if(ioctl(fd, EVIOCGABS(ABS_X), &absInfo) >= 0){
                tabletData->minValues.x = absInfo.minimum;
                tabletData->maxValues.x = absInfo.maximum;
            }
            if(ioctl(fd, EVIOCGABS(ABS_Y), &absInfo) >= 0){
                tabletData->minValues.y = absInfo.minimum;
                tabletData->maxValues.y = absInfo.maximum;
            }
            if(ioctl(fd, EVIOCGABS(ABS_PRESSURE), &absInfo) >= 0){
                tabletData->minValues.p = absInfo.minimum;
                tabletData->maxValues.p = absInfo.maximum;
            }
            if(ioctl(fd, EVIOCGABS(ABS_DISTANCE), &absInfo) >= 0){
                tabletData->minValues.d = absInfo.minimum;
                tabletData->maxValues.d = absInfo.maximum;
            }
            break;
        }
        case QInputDeviceManager::DeviceTypeTouch:{
            auto touchData = new TouchData();
            m_data = touchData;
            long absbits[NUM_LONGS(ABS_CNT)];
            if(ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbits)), absbits) >= 0){
                touchData->isTypeB = testBit(ABS_MT_SLOT, absbits);
                touchData->singleTouch = !testBit(ABS_MT_POSITION_X, absbits);
            }
            input_absinfo absInfo;
            memset(&absInfo, 0, sizeof(input_absinfo));
            if(ioctl(fd, EVIOCGABS((touchData->singleTouch ? ABS_X : ABS_MT_POSITION_X)), &absInfo) >= 0){
                touchData->minX = absInfo.minimum;
                touchData->maxX = absInfo.maximum;
            }
            if(ioctl(fd, EVIOCGABS((touchData->singleTouch ? ABS_Y : ABS_MT_POSITION_Y)), &absInfo) >= 0){
                touchData->minY = absInfo.minimum;
                touchData->maxY = absInfo.maximum;
            }
            if(ioctl(fd, EVIOCGABS(ABS_PRESSURE), &absInfo) >= 0 && absInfo.maximum > absInfo.minimum){
                touchData->minPressure = absInfo.minimum;
                touchData->maxPressure = absInfo.maximum;
            }
            auto touchDevice = new QTouchDevice;
            touchDevice->setName(hw_name);
            touchDevice->setType(QTouchDevice::TouchScreen);
            touchDevice->setCapabilities(QTouchDevice::Position | QTouchDevice::Area);
            if(touchData->maxPressure > touchData->minPressure){
                touchDevice->setCapabilities(touchDevice->capabilities() | QTouchDevice::Pressure);
            }
            if(ioctl(fd, EVIOCGABS(ABS_MT_SLOT), &absInfo)){
                touchDevice->setMaximumTouchPoints(absInfo.maximum);
            }
            touchData->device = touchDevice;
            QWindowSystemInterface::registerTouchDevice(touchDevice);
            break;
        }
        case QInputDeviceManager::DeviceTypePointer:
            set<PointerData>();
            break;
        default:
            break;
    }
    ::close(fd);
}

DeviceData::~DeviceData(){
    switch(type){
        case QInputDeviceManager::DeviceTypeKeyboard:
            delete get<KeyboardData>();
            break;
        case QInputDeviceManager::DeviceTypeTablet:
            delete get<TabletData>();
            break;
        case QInputDeviceManager::DeviceTypeTouch:
            delete get<TouchData>();
            break;
        case QInputDeviceManager::DeviceTypePointer:
            delete get<PointerData>();
            break;
        default:
            break;
    }
}


OxideEventHandler::OxideEventHandler(OxideEventManager* manager, const QStringList& parameters)
: QDaemonThread(),
  m_manager(manager),
  m_fd(Blight::connection()->input_handle()),
  m_keymap{0},
  m_keymap_size{0},
  m_keycompose{0},
  m_keycompose_size{0}
{
    setObjectName("OxideInput");
    moveToThread(this);
    parseKeyParams(parameters);
    parseTouchParams(QString(deviceSettings.getTouchEnvSetting()).split(QLatin1Char(':')));
    parseTouchParams(parameters);
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read);
    m_notifier->moveToThread(this);
    connect(m_notifier, &QSocketNotifier::activated, this, &OxideEventHandler::readyRead);
    start();
}

OxideEventHandler::~OxideEventHandler(){
    m_notifier->setEnabled(false);
    m_notifier->deleteLater();
    quit();
    wait();
}

void OxideEventHandler::add(unsigned int number, QInputDeviceManager::DeviceType type){
    if(!m_devices.contains(number)){
        m_devices.insert(number, new DeviceData(number, type));
    }
}

void OxideEventHandler::remove(unsigned int number, QInputDeviceManager::DeviceType type){
    if(m_devices.contains(number) && m_devices[number]->type == type){
        auto device = m_devices[number];
        m_devices.remove(number);
        delete device;
    }
}

void OxideEventHandler::readyRead(){
    Q_ASSERT(thread() == QThread::currentThread());
    Q_ASSERT(m_notifier->thread() == QThread::currentThread());
    m_notifier->setEnabled(false);
    auto connection = Blight::connection();
    while(true){
        auto maybe = connection->read_event();
        if(!maybe.has_value()){
            if(errno != EAGAIN && errno != EINTR){
                O_WARNING("Failed reading input stream" << strerror(errno));
                return;
            }
            break;
        }
        const auto& device = maybe.value().device;
        if(!m_devices.contains(device)){
            continue;
        }
        auto& event = maybe.value().event;
        auto data = m_devices[device];
        switch(data->type){
            case QInputDeviceManager::DeviceTypeKeyboard:
                processKeyboardEvent(data, &event);
                break;
            case QInputDeviceManager::DeviceTypeTablet:
                processTabletEvent(data, &event);
                break;
            case QInputDeviceManager::DeviceTypeTouch:
                processTouchEvent(data, &event);
                break;
            case QInputDeviceManager::DeviceTypePointer:
                processPointerEvent(data, &event);
                break;
            default:
                continue;
        }
    }
    m_notifier->setEnabled(true);
}

void OxideEventHandler::processKeyboardEvent(
    DeviceData* data,
    Blight::partial_input_event_t* event
){
    Q_ASSERT(data->type == QInputDeviceManager::DeviceTypeKeyboard);
    // Ignore EV_SYN etc
    if(event->type != EV_KEY){
        return;
    }
    auto keyboardData = data->get<KeyboardData>();
    auto& keycode = event->code;
    bool pressed = event->value;
    bool autorepeat = event->value == 2;

    bool first_press = pressed && !autorepeat;
    const QEvdevKeyboardMap::Mapping* map_plain = 0;
    const QEvdevKeyboardMap::Mapping* map_withmod = 0;
    quint8 modifiers = keyboardData->m_modifiers;

    for(int i = 0; i < m_keymap_size && !(map_plain && map_withmod); ++i){
        const QEvdevKeyboardMap::Mapping* m = m_keymap + i;
        if(m->keycode == keycode){
            if(m->modifiers == 0){
                map_plain = m;
            }
            quint8 testmods = keyboardData->m_modifiers;
            if(
                keyboardData->m_locks[0] /*CapsLock*/
                && (m->flags & QEvdevKeyboardMap::IsLetter)
            ){
                testmods ^= QEvdevKeyboardMap::ModShift;
            }
            if(m->modifiers == testmods){
                map_withmod = m;
            }
        }
    }

    if(
        keyboardData->m_locks[0] /*CapsLock*/
        && map_withmod
        && (map_withmod->flags & QEvdevKeyboardMap::IsLetter)
    ){
        modifiers ^= QEvdevKeyboardMap::ModShift;
    }
    const QEvdevKeyboardMap::Mapping* it = map_withmod ? map_withmod : map_plain;
    // we couldn't even find a plain mapping
    if(!it){
        O_WARNING("No mapping for keycode found:" << keycode);
        return;
    }
    quint16 unicode = it->unicode;
    quint32 qtcode = it->qtcode;
    if(
        it->flags & QEvdevKeyboardMap::IsModifier
        && it->special
    ){
        // this is a modifier, i.e. Shift, Alt, ...
        if(pressed){
            keyboardData->m_modifiers |= it->special;
        }else{
            keyboardData->m_modifiers &= ~it->special;
        }
    } else if(qtcode >= Qt::Key_CapsLock && qtcode <= Qt::Key_ScrollLock){
        // (Caps|Num|Scroll)Lock
        if(first_press){
            quint8& lock = keyboardData->m_locks[qtcode - Qt::Key_CapsLock];
            lock ^= 1;
        }
    }else if((it->flags & QEvdevKeyboardMap::IsSystem) && it->special && first_press){
        switch(it->special){
            case QEvdevKeyboardMap::SystemZap:
                if(!m_no_zap){
                    qApp->quit();
                }
                break;
        }
        return;
    }else if(qtcode == Qt::Key_Multi_key && m_do_compose){
        // the Compose key was pressed
        if(first_press){
            keyboardData->m_composing = 2;
        }
        return;
    } else if(it->flags & QEvdevKeyboardMap::IsDead && m_do_compose){
        // a Dead key was pressed twice
        if(
            first_press
            && keyboardData->m_composing == 1
            && keyboardData->m_dead_unicode == unicode
        ){
            keyboardData->m_composing = 0;
            // otherwise it would be Qt::Key_Dead...
            qtcode = Qt::Key_unknown;
        } else if(first_press && unicode != 0xffff){
            keyboardData->m_dead_unicode = unicode;
            keyboardData->m_composing = 1;
            return;
        }else{
            return;
        }
    }
    // a normal key was pressed
    const int modmask = Qt::ShiftModifier
        | Qt::ControlModifier
        | Qt::AltModifier
        | Qt::MetaModifier
        | Qt::KeypadModifier;
    // we couldn't find a specific mapping for the current modifiers,
    // or that mapping didn't have special modifiers:
    // so just report the plain mapping with additional modifiers.
    if(
        (
            it == map_plain
            && it != map_withmod
        )
        || (
            map_withmod
            && !(map_withmod->qtcode & modmask)
        )
    ) {
        qtcode |= toQtModifiers(modifiers);
    }
    if(
        keyboardData->m_composing == 2
        && first_press
        && !(it->flags & QEvdevKeyboardMap::IsModifier)
    ){
        // the last key press was the Compose key
        if(unicode != 0xffff){
            int idx = 0;
            // check if this code is in the compose table at all
            for( ; idx < m_keycompose_size; ++idx){
                if(m_keycompose[idx].first == unicode){
                    break;
                }
            }
            if(idx < m_keycompose_size){
                // found it -> simulate a Dead key press
                keyboardData->m_dead_unicode = unicode;
                unicode = 0xffff;
                keyboardData->m_composing = 1;
                return;
            }else{
                keyboardData->m_composing = 0;
            }
        }else{
            keyboardData->m_composing = 0;
        }
    }else if(
        keyboardData->m_composing == 1
        && first_press
        && !(it->flags & QEvdevKeyboardMap::IsModifier)
    ){
        // the last key press was a Dead key
        bool valid = false;
        if(unicode != 0xffff){
            int idx = 0;
            // check if this code is in the compose table at all
            for( ; idx < m_keycompose_size; ++idx){
                if(
                    m_keycompose[idx].first == keyboardData->m_dead_unicode
                    && m_keycompose[idx].second == unicode
                ){
                    break;
                }
            }
            if(idx < m_keycompose_size){
                quint16 composed = m_keycompose[idx].result;
                if(composed != 0xffff){
                    unicode = composed;
                    qtcode = Qt::Key_unknown;
                    valid = true;
                }
            }
        }
        if(!valid){
            unicode = keyboardData->m_dead_unicode;
            qtcode = Qt::Key_unknown;
        }
        keyboardData->m_composing = 0;
    }
    // Up until now qtcode contained both the key and modifiers. Split it.
    Qt::KeyboardModifiers qtmods = Qt::KeyboardModifiers(qtcode & modmask);
    qtcode &= ~modmask;
    // If NumLockOff and keypad key pressed remap event sent
    if(
        !keyboardData->m_locks[1]
        && qtmods & Qt::KeypadModifier
        && keycode >= 71
        && keycode <= 83
        && keycode != 74
        && keycode != 78
    ){
        unicode = 0xffff;
        switch(keycode){
            case 71: //7 --> Home
                qtcode = Qt::Key_Home;
                break;
            case 72: //8 --> Up
                qtcode = Qt::Key_Up;
                break;
            case 73: //9 --> PgUp
                qtcode = Qt::Key_PageUp;
                break;
            case 75: //4 --> Left
                qtcode = Qt::Key_Left;
                break;
            case 76: //5 --> Clear
                qtcode = Qt::Key_Clear;
                break;
            case 77: //6 --> right
                qtcode = Qt::Key_Right;
                break;
            case 79: //1 --> End
                qtcode = Qt::Key_End;
                break;
            case 80: //2 --> Down
                qtcode = Qt::Key_Down;
                break;
            case 81: //3 --> PgDn
                qtcode = Qt::Key_PageDown;
                break;
            case 82: //0 --> Ins
                qtcode = Qt::Key_Insert;
                break;
            case 83: //, --> Del
                qtcode = Qt::Key_Delete;
                break;
        }
    }
    // Map SHIFT + Tab to SHIFT + Backtab, QShortcutMap knows about this translation
    if(
        qtcode == Qt::Key_Tab
        && (qtmods & Qt::ShiftModifier) == Qt::ShiftModifier
    ){
        qtcode = Qt::Key_Backtab;
    }
    // Generate the QPA event.
    if(!autorepeat){
        QGuiApplicationPrivate::inputDeviceManager()->setKeyboardModifiers(
            toQtModifiers(keyboardData->m_modifiers)
        );
    }
    qDebug() << toQtModifiers(keyboardData->m_modifiers) << toQtModifiers(it->special) << it->special;
    QEvent::Type type = pressed ? QEvent::KeyPress : QEvent::KeyRelease;
    QString text = unicode != 0xffff ? QString(QChar(unicode)) : QString();
    QWindowSystemInterface::handleExtendedKeyEvent(
        nullptr,
        type,
        qtcode,
        qtmods,
        keycode + 8,
        0,
        int(modifiers),
        text,
        autorepeat
    );
}

void OxideEventHandler::processTabletEvent(
    DeviceData* data,
    Blight::partial_input_event_t* event
){
    Q_ASSERT(data->type == QInputDeviceManager::DeviceTypeTablet);
    auto device = data->device;
    auto tabletData = data->get<TabletData>();
    // TODO handle tilt
    if(event->type == EV_ABS){
        switch (event->code){
            case ABS_X:
                tabletData->state.x = event->value;
                break;
            case ABS_Y:
                tabletData->state.y = event->value;
                break;
            case ABS_PRESSURE:
                tabletData->state.p = event->value;
                break;
            case ABS_DISTANCE:
                tabletData->state.d = event->value;
                break;
            default:
                break;
        }
    }else if(event->type == EV_KEY){
        // code BTN_TOOL_* value 1 -> proximity enter
        // code BTN_TOOL_* value 0 -> proximity leave
        // code BTN_TOUCH value 1 -> contact with screen
        // code BTN_TOUCH value 0 -> no contact
        switch (event->code) {
            case BTN_TOUCH:
                tabletData->state.down = event->value != 0;
                break;
            case BTN_TOOL_PEN:
                tabletData->state.tool = event->value ? QTabletEvent::Pen : 0;
                break;
            case BTN_TOOL_RUBBER:
                tabletData->state.tool = event->value ? QTabletEvent::Eraser : 0;
                break;
            default:
                break;
        }
    }else if(event->type == EV_SYN && event->code == SYN_REPORT && tabletData->lastEventType != event->type){
        if(!tabletData->state.lastReportTool && tabletData->state.tool){
            QWindowSystemInterface::handleTabletEnterProximityEvent(
                QTabletEvent::Stylus,
                tabletData->state.tool,
                device
            );
        }
        qreal nx = (tabletData->state.x - tabletData->minValues.x) / qreal(tabletData->maxValues.x - tabletData->minValues.x);
        qreal ny = (tabletData->state.y - tabletData->minValues.y) / qreal(tabletData->maxValues.y - tabletData->minValues.y);

        QRect winRect = QGuiApplication::primaryScreen()->geometry();
        QPointF globalPos(nx * winRect.width(), ny * winRect.height());
        int pointer = tabletData->state.tool;
        // Prevent sending confusing values of 0 when moving the pen outside the active area.
        if(!tabletData->state.down && tabletData->state.lastReportDown){
            globalPos = tabletData->state.lastReportPos;
            pointer = tabletData->state.lastReportTool;
        }
        int pressureRange = tabletData->maxValues.p - tabletData->minValues.p;
        qreal pressure = pressureRange
            ? (tabletData->state.p - tabletData->minValues.p) / qreal(pressureRange)
            : qreal(1);
        if(tabletData->state.down || tabletData->state.lastReportDown){
            auto button = tabletData->state.down ? Qt::LeftButton : Qt::NoButton;
            QWindowSystemInterface::handleTabletEvent(
                nullptr,
                QPointF(),
                globalPos,
                QTabletEvent::Stylus,
                pointer,
                button,
                pressure,
                0,
                0,
                0,
                0,
                0,
                device,
                qGuiApp->keyboardModifiers()
            );
        }
        if(tabletData->state.lastReportTool && !tabletData->state.tool){
            QWindowSystemInterface::handleTabletLeaveProximityEvent(
                QTabletEvent::Stylus,
                tabletData->state.tool,
                device
            );
        }
        tabletData->state.lastReportDown = tabletData->state.down;
        tabletData->state.lastReportTool = tabletData->state.tool;
        tabletData->state.lastReportPos = globalPos;
    }
    tabletData->lastEventType = event->type;
}

void addTouchPoint(QTransform rotate, TouchData* touchData, const Contact& contact, Qt::TouchPointStates* combinedStates){
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
    tp.normalPosition = QPointF(
        (contact.x - touchData->minX)
            / qreal(touchData->maxX - touchData->minX),
        (contact.y - touchData->minY)
            / qreal(touchData->maxY - touchData->minY)
    );
    if(!rotate.isIdentity()){
        tp.normalPosition = rotate.map(tp.normalPosition);
    }
    tp.rawPositions.append(QPointF(contact.x, contact.y));
    touchData->touchPoints.append(tp);
}

int findClosestContact(const QHash<int, Contact> &contacts, int x, int y, int *dist){
    int minDist = -1, id = -1;
    for(QHash<int, Contact>::const_iterator it = contacts.constBegin(), ite = contacts.constEnd();
         it != ite; ++it) {
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

void OxideEventHandler::processTouchEvent(
    DeviceData* data,
    Blight::partial_input_event_t* event
){
    Q_ASSERT(data->type == QInputDeviceManager::DeviceTypeTouch);
    auto touchData = data->get<TouchData>();
    if(event->type == EV_ABS){
        if(event->code == ABS_MT_POSITION_X || (touchData->singleTouch && event->code == ABS_X)){
            touchData->currentData.x = qBound(touchData->minX, event->value, touchData->maxX);
            if(touchData->singleTouch){
                touchData->contacts[touchData->currentSlot].x = touchData->currentData.x;
            }
            if(touchData->isTypeB){
                touchData->contacts[touchData->currentSlot].x = touchData->currentData.x;
                if(touchData->contacts[touchData->currentSlot].state == Qt::TouchPointStationary){
                    touchData->contacts[touchData->currentSlot].state = Qt::TouchPointMoved;
                }
            }
        }else if(event->code == ABS_MT_POSITION_Y || (touchData->singleTouch && event->code == ABS_Y)){
            touchData->currentData.y = qBound(touchData->minY, event->value, touchData->maxY);
            if(touchData->singleTouch){
                touchData->contacts[touchData->currentSlot].y = touchData->currentData.y;
            }
            if(touchData->isTypeB){
                touchData->contacts[touchData->currentSlot].y = touchData->currentData.y;
                if(touchData->contacts[touchData->currentSlot].state == Qt::TouchPointStationary){
                    touchData->contacts[touchData->currentSlot].state = Qt::TouchPointMoved;
                }
            }
        }else if(event->code == ABS_MT_TRACKING_ID){
            touchData->currentData.trackingId = event->value;
            if(touchData->isTypeB){
                if(touchData->currentData.trackingId == -1){
                    touchData->contacts[touchData->currentSlot].state = Qt::TouchPointReleased;
                }else{
                    touchData->contacts[touchData->currentSlot].state = Qt::TouchPointPressed;
                    touchData->contacts[touchData->currentSlot].trackingId = touchData->currentData.trackingId;
                }
            }
        }else if(event->code == ABS_MT_TOUCH_MAJOR){
            touchData->currentData.maj = event->value;
            if(event->value == 0){
                touchData->currentData.state = Qt::TouchPointReleased;
            }
            if(touchData->isTypeB){
                touchData->contacts[touchData->currentSlot].maj = touchData->currentData.maj;
            }
        }else if(event->code == ABS_PRESSURE || event->code == ABS_MT_PRESSURE){
            touchData->currentData.pressure = qBound(touchData->minPressure, event->value, touchData->maxPressure);
            if(touchData->isTypeB || touchData->singleTouch){
                touchData->contacts[touchData->currentSlot].pressure = touchData->currentData.pressure;
            }
        }else if(event->code == ABS_MT_SLOT){
            touchData->currentSlot = event->value;
        }
    }else if(event->type == EV_KEY && !touchData->isTypeB){
        if(event->code == BTN_TOUCH && event->value == 0){
            touchData->contacts[touchData->currentSlot].state = Qt::TouchPointReleased;
        }
    }else if(event->type == EV_SYN && event->code == SYN_MT_REPORT && touchData->lastEventType != EV_SYN){
        // If there is no tracking id, one will be generated later.
        // Until that use a temporary key.
        int key = touchData->currentData.trackingId;
        if(key == -1){
            key = touchData->contacts.count();
        }
        touchData->contacts.insert(key, touchData->currentData);
        touchData->currentData = Contact();
    }else if(event->type == EV_SYN && event->code == SYN_REPORT){
        // Ensure valid IDs even when the driver does not report ABS_MT_TRACKING_ID.
        if(!touchData->contacts.isEmpty() && touchData->contacts.constBegin().value().trackingId == -1){
            QHash<int, Contact> candidates = touchData->lastContacts, pending = touchData->contacts, newContacts;
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
            touchData->contacts = newContacts;
        }
        // update timestamps
        touchData->lastTimeStamp = touchData->timeStamp;
        touchData->timeStamp = std::chrono::duration<double>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count();

        touchData->lastTouchPoints = touchData->touchPoints;
        touchData->touchPoints.clear();
        Qt::TouchPointStates combinedStates;
        bool hasPressure = false;
        for(auto i = touchData->contacts.begin(), end = touchData->contacts.end(); i != end; /*erasing*/){
            auto it = i++;
            Contact &contact(it.value());
            if(!contact.state){
                continue;
            }
            int key = touchData->isTypeB ? it.key() : contact.trackingId;
            if(!touchData->isTypeB && touchData->lastContacts.contains(key)){
                const Contact &prev(touchData->lastContacts.value(key));
                if(contact.state == Qt::TouchPointReleased){
                    // Copy over the previous values for released points, just in case.
                    contact.x = prev.x;
                    contact.y = prev.y;
                    contact.maj = prev.maj;
                }else{
                    contact.state = prev.x == contact.x && prev.y == contact.y
                        ? Qt::TouchPointStationary
                        : Qt::TouchPointMoved;
                }
            }
            // Avoid reporting a contact in released state more than once.
            if(!touchData->isTypeB && contact.state == Qt::TouchPointReleased && !touchData->lastContacts.contains(key)){
                touchData->contacts.erase(it);
                continue;
            }
            if(contact.pressure){
                hasPressure = true;
            }
            addTouchPoint(m_rotate, touchData, contact, &combinedStates);
        }
        // Now look for contacts that have disappeared since the last sync.
        for(auto it = touchData->lastContacts.begin(), end = touchData->lastContacts.end(); it != end; ++it){
            Contact &contact(it.value());
            int key = touchData->isTypeB ? it.key() : contact.trackingId;
            if(touchData->isTypeB){
                if(contact.trackingId != touchData->contacts[key].trackingId && contact.state){
                    contact.state = Qt::TouchPointReleased;
                    addTouchPoint(m_rotate, touchData, contact, &combinedStates);
                }
            }else if(!touchData->contacts.contains(key)){
                contact.state = Qt::TouchPointReleased;
                addTouchPoint(m_rotate, touchData, contact, &combinedStates);
            }
        }
        // Remove contacts that have just been reported as released.
        for(auto i = touchData->contacts.begin(), end = touchData->contacts.end(); i != end; /*erasing*/){
            auto it = i++;
            Contact &contact(it.value());
            if(!contact.state){
                continue;
            }
            if(contact.state != Qt::TouchPointReleased){
                contact.state = Qt::TouchPointStationary;
            }else if(touchData->isTypeB){
                contact.state = static_cast<Qt::TouchPointState>(0);
            }else{
                touchData->contacts.erase(it);
            }
        }
        touchData->lastContacts = touchData->contacts;
        if(!touchData->isTypeB && !touchData->singleTouch){
            touchData->contacts.clear();
        }
        if(!touchData->touchPoints.isEmpty() && (hasPressure || combinedStates != Qt::TouchPointStationary)){
            auto screen = QGuiApplication::primaryScreen();
            QRect winRect = QHighDpi::toNativePixels(screen->geometry(), screen);
            if(winRect.isNull()){
                return;
            }
            const int hw_w = touchData->maxX - touchData->minX;
            const int hw_h = touchData->maxY - touchData->minY;
            // Map the coordinates based on the normalized position. QPA expects 'area'
            // to be in screen coordinates.
            const int pointCount = touchData->touchPoints.count();
            for(int i = 0; i < pointCount; ++i){
                QWindowSystemInterface::TouchPoint &tp(touchData->touchPoints[i]);
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
                if(!touchData->minPressure && !touchData->maxPressure){
                    tp.pressure = tp.state == Qt::TouchPointReleased ? 0 : 1;
                }else{
                    tp.pressure = (tp.pressure - touchData->minPressure) / qreal(touchData->maxPressure - touchData->minPressure);
                }
            }
            // Let qguiapp pick the target window.
            QWindowSystemInterface::handleTouchEvent(nullptr, touchData->device, touchData->touchPoints);
        }
    }
    touchData->lastEventType = event->type;
}

void OxideEventHandler::processPointerEvent(
    DeviceData* data,
    Blight::partial_input_event_t* event
){
    Q_ASSERT(data->type == QInputDeviceManager::DeviceTypePointer);
    auto device = data->device;
    auto pointerData = data->get<PointerData>();
}

Qt::KeyboardModifiers OxideEventHandler::toQtModifiers(quint16 mod){
    Qt::KeyboardModifiers qtmod = Qt::NoModifier;
    if(mod & (Modifiers::ModShift | Modifiers::ModShiftL | Modifiers::ModShiftR)){
        qtmod |= Qt::ShiftModifier;
    }
    if(mod & (Modifiers::ModControl | Modifiers::ModCtrlL | Modifiers::ModCtrlR)){
        qtmod |= Qt::ControlModifier;
    }
    if(mod & Modifiers::ModAlt || mod & Modifiers::ModAltGr){
        qtmod |= Qt::AltModifier;
    }
    if(mod & Modifiers::ModMeta){
        qtmod |= Qt::MetaModifier;
    }
    return qtmod;
}

void OxideEventHandler::unloadKeymap(){
    if(m_keymap && m_keymap != s_keymap_default){
        delete [] m_keymap;
    }
    if(m_keycompose && m_keycompose != s_keycompose_default){
        delete [] m_keycompose;
    }
    m_keymap = s_keymap_default;
    m_keymap_size = sizeof(s_keymap_default) / sizeof(s_keymap_default[0]);
    m_keycompose = s_keycompose_default;
    m_keycompose_size = sizeof(s_keycompose_default) / sizeof(s_keycompose_default[0]);
}

bool OxideEventHandler::loadKeymap(const QString& file){
    QFile f(file);
    if(!f.open(QIODevice::ReadOnly)){
        O_WARNING("Could not open keymap file" << qUtf16Printable(file));
        return false;
    }
    quint32 qmap_magic, qmap_version, qmap_keymap_size, qmap_keycompose_size;
    QDataStream ds(&f);
    ds >> qmap_magic >> qmap_version >> qmap_keymap_size >> qmap_keycompose_size;
    if(
        ds.status() != QDataStream::Ok
        || qmap_magic != QEvdevKeyboardMap::FileMagic
        || qmap_version != 1
        || qmap_keymap_size == 0
    ){
        O_WARNING(qUtf16Printable(file) << " is not a valid .qmap keymap file");
        return false;
    }
    QEvdevKeyboardMap::Mapping *qmap_keymap = new QEvdevKeyboardMap::Mapping[qmap_keymap_size];
    QEvdevKeyboardMap::Composing *qmap_keycompose = qmap_keycompose_size
        ? new QEvdevKeyboardMap::Composing[qmap_keycompose_size]
        : 0;
    for(quint32 i = 0; i < qmap_keymap_size; ++i){
        ds >> qmap_keymap[i];
    }
    for(quint32 i = 0; i < qmap_keycompose_size; ++i){
        ds >> qmap_keycompose[i];
    }
    if(ds.status() != QDataStream::Ok){
        delete [] qmap_keymap;
        delete [] qmap_keycompose;
        O_WARNING("Keymap file" << qUtf16Printable(file) << "cannot be loaded.");
        return false;
    }
    // unload currently active and clear state
    unloadKeymap();

    m_keymap = qmap_keymap;
    m_keymap_size = qmap_keymap_size;
    m_keycompose = qmap_keycompose;
    m_keycompose_size = qmap_keycompose_size;
    m_do_compose = true;
    return true;
}

void OxideEventHandler::parseKeyParams(const QStringList& parameters){
    QString keymap;
    for(const QString& param : parameters){
        if(param.startsWith(QLatin1String("keymap="), Qt::CaseInsensitive)){
            keymap = param.section(QLatin1Char('='), 1, 1);
        }
    }
    if(keymap.isEmpty() || !loadKeymap(keymap)){
        unloadKeymap();
    }
}

void OxideEventHandler::parseTouchParams(const QStringList& parameters){
    int rotationAngle = 0;
    bool invertx = false;
    bool inverty = false;
    for(const QString& param : parameters){
        if(param.startsWith(QLatin1String("rotate"))){
            QString rotateArg = param.section(QLatin1Char('='), 1, 1);
            bool ok;
            uint argValue = rotateArg.toUInt(&ok);
            if(ok){
                switch(argValue){
                    case 90:
                    case 180:
                    case 270:
                        rotationAngle = argValue;
                    default:
                        break;
                }
            }
        }else if(param == QLatin1String("invertx")){
            invertx = true;
        }else if(param == QLatin1String("inverty")){
            inverty = true;
        }
    }
    if(rotationAngle){
        m_rotate = QTransform::fromTranslate(0.5, 0.5).rotate(rotationAngle).translate(-0.5, -0.5);
    }
    if(invertx){
        m_rotate *= QTransform::fromTranslate(0.5, 0.5).scale(-1.0, 1.0).translate(-0.5, -0.5);
    }
    if(inverty){
        m_rotate *= QTransform::fromTranslate(0.5, 0.5).scale(1.0, -1.0).translate(-0.5, -0.5);
    }
}
