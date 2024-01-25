#include "oxideeventhandler.h"
#include "oxideeventmanager.h"
#include "default_keymap.h"

#include <libblight.h>
#include <liboxide/debug.h>

typedef struct KeyboardData {
    quint8 m_modifiers;
    quint8 m_locks[3];
    int m_composing;
    quint16 m_dead_unicode;
    quint8 m_langLock;
} KeyboardData;

typedef struct TabletData {
    struct {
        int x, y, p, d;
    } minValues, maxValues;
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

typedef struct TouchData {

} TouchData;

typedef struct PointerData {

} PointerData;

DeviceData::DeviceData() : DeviceData(0, QInputDeviceManager::DeviceTypeUnknown){}

DeviceData::DeviceData(unsigned int device, QInputDeviceManager::DeviceType type)
    : device(device),
  type(type)
{
    switch(type){
        case QInputDeviceManager::DeviceTypeKeyboard:{
            auto keyboardData = set<KeyboardData>();
            keyboardData->m_dead_unicode = 0xffff;
            break;
        }
        case QInputDeviceManager::DeviceTypeTablet:
            m_data = new TabletData();
            break;
        case QInputDeviceManager::DeviceTypeTouch:
            set<TouchData>();
            break;
        case QInputDeviceManager::DeviceTypePointer:
            set<PointerData>();
            break;
        default:
            break;
    }
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
: m_manager(manager),
  m_fd(Blight::connection()->input_handle()),
  m_keymap{0},
  m_keymap_size{0},
  m_keycompose{0},
  m_keycompose_size{0}
{
    QString keymap;
    for(const QString& param : parameters){
        if(param.startsWith(QLatin1String("keymap="), Qt::CaseInsensitive)){
            keymap = param.split('=').last();
        }
    }
    if(keymap.isEmpty() || !loadKeymap(keymap)){
        unloadKeymap();
    }
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &OxideEventHandler::readyRead);
    start();
}

OxideEventHandler::~OxideEventHandler(){
    quit();
    wait();
}

void OxideEventHandler::add(unsigned int number, QInputDeviceManager::DeviceType type){
    if(!m_devices.contains(number)){
        m_devices.insert(number, DeviceData(number, type));
    }
}

void OxideEventHandler::remove(unsigned int number, QInputDeviceManager::DeviceType type){
    if(m_devices.contains(number) && m_devices[number].type == type){
        m_devices.remove(number);
    }
}

void OxideEventHandler::readyRead(){
    m_notifier->setEnabled(false);
    while(true){
        auto maybe = Blight::connection()->read_event();
        if(!maybe.has_value()){
            if(errno == EAGAIN || errno == EINTR){
                timespec remaining;
                timespec requested{
                    .tv_sec = 0,
                    .tv_nsec = 5000
                };
                nanosleep(&requested, &remaining);
                continue;
            }
            break;
        }
        const auto& device = maybe.value().device;
        if(!m_devices.contains(device)){
            continue;
        }
        auto& event = maybe.value().event;
        auto data = &m_devices[device];
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

void OxideEventHandler::processKeyboardEvent(DeviceData* data, input_event* event){
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
            if(keyboardData->m_langLock){
                testmods ^= QEvdevKeyboardMap::ModAltGr;
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
        O_WARNING("No mapping for keycode found");
        return;
    }
    bool skip = false;
    quint16 unicode = it->unicode;
    quint32 qtcode = it->qtcode;
    if(
        it->flags & QEvdevKeyboardMap::IsModifier
        && it->special
    ){
        // this is a modifier, i.e. Shift, Alt, ...
        if(pressed){
            keyboardData->m_modifiers |= quint8(it->special);
        }else{
            keyboardData->m_modifiers &= ~quint8(it->special);
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
        skip = true; // no need to tell Qt about it
    }else if(qtcode == Qt::Key_Multi_key && m_do_compose){
        // the Compose key was pressed
        if(first_press){
            keyboardData->m_composing = 2;
        }
        skip = true;
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
            skip = true;
        }else{
            skip = true;
        }
    }
    if(!skip){
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
            qtcode |= QEvdevKeyboardHandler::toQtModifiers(modifiers);
        }
        if(
            keyboardData->m_composing == 2
            && first_press
            && !(it->flags & QEvdevKeyboardMap::IsModifier)
        ) {
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
                    skip = true;
                } else {
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
        qDebug() << keycode << event->value << skip;
        if(!skip){
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
            if(qtcode == Qt::Key_Tab && (qtmods & Qt::ShiftModifier) == Qt::ShiftModifier){
                qtcode = Qt::Key_Backtab;
            }
            // Generate the QPA event.
            if(!autorepeat){
                QGuiApplicationPrivate::inputDeviceManager()->setKeyboardModifiers(
                    QEvdevKeyboardHandler::toQtModifiers(keyboardData->m_modifiers)
                );
            }
            QWindowSystemInterface::handleExtendedKeyEvent(
                0,
                pressed
                    ? QEvent::KeyPress
                    : QEvent::KeyRelease,
                qtcode,
                qtmods,
                keycode + 8, 0,
                int(modifiers),
                unicode != 0xffff
                    ? QString(QChar(unicode))
                    : QString(), autorepeat
            );
        }
    }
}

void OxideEventHandler::processTabletEvent(DeviceData* data, input_event* event){
    Q_ASSERT(data->type == QInputDeviceManager::DeviceTypeTablet);
    auto device = data->device;
    auto tabletData = data->get<TabletData>();
    auto& state = tabletData->state;
    auto& lastEventType = tabletData->lastEventType;
    auto& minValues = tabletData->minValues;
    auto& maxValues = tabletData->maxValues;
    if(event->type == EV_ABS){
        switch (event->code) {
            case ABS_X:
                state.x = event->value;
                break;
            case ABS_Y:
                state.y = event->value;
                break;
            case ABS_PRESSURE:
                state.p = event->value;
                break;
            case ABS_DISTANCE:
                state.d = event->value;
                break;
            default:
                break;
        }
    }else if (event->type == EV_KEY){
        // code BTN_TOOL_* value 1 -> proximity enter
        // code BTN_TOOL_* value 0 -> proximity leave
        // code BTN_TOUCH value 1 -> contact with screen
        // code BTN_TOUCH value 0 -> no contact
        switch (event->code) {
            case BTN_TOUCH:
                state.down = event->value != 0;
                break;
            case BTN_TOOL_PEN:
                state.tool = event->value ? QTabletEvent::Pen : 0;
                break;
            case BTN_TOOL_RUBBER:
                state.tool = event->value ? QTabletEvent::Eraser : 0;
                break;
            default:
                break;
        }
    }else if(event->type == EV_SYN && event->code == SYN_REPORT && lastEventType != event->type){
        if(!state.lastReportTool && state.tool){
            QWindowSystemInterface::handleTabletEnterProximityEvent(
                QTabletEvent::Stylus,
                state.tool,
                device
            );
        }
        qreal nx = (state.x - minValues.x) / qreal(maxValues.x - minValues.x);
        qreal ny = (state.y - minValues.y) / qreal(maxValues.y - minValues.y);

        QRect winRect = QGuiApplication::primaryScreen()->geometry();
        QPointF globalPos(nx * winRect.width(), ny * winRect.height());
        int pointer = state.tool;
        // Prevent sending confusing values of 0 when moving the pen outside the active area.
        if (!state.down && state.lastReportDown) {
            globalPos = state.lastReportPos;
            pointer = state.lastReportTool;
        }
        int pressureRange = maxValues.p - minValues.p;
        qreal pressure = pressureRange
            ? (state.p - minValues.p) / qreal(pressureRange)
            : qreal(1);
        if(state.down || state.lastReportDown){
            QWindowSystemInterface::handleTabletEvent(
                0,
                QPointF(),
                globalPos,
                QTabletEvent::Stylus, pointer,
                state.down ? Qt::LeftButton : Qt::NoButton,
                pressure,
                0, 0, 0, 0, 0,
                device,
                qGuiApp->keyboardModifiers()
            );
        }
        if(state.lastReportTool && !state.tool){
            QWindowSystemInterface::handleTabletLeaveProximityEvent(
                QTabletEvent::Stylus,
                state.tool,
                device
            );
        }
        state.lastReportDown = state.down;
        state.lastReportTool = state.tool;
        state.lastReportPos = globalPos;
    }
    lastEventType = event->type;
}

void OxideEventHandler::processTouchEvent(DeviceData* data, input_event* event){
    Q_ASSERT(data->type == QInputDeviceManager::DeviceTypeTouch);
    auto device = data->device;
    auto touchData = data->get<TouchData>();
}

void OxideEventHandler::processPointerEvent(DeviceData* data, input_event* event){
    Q_ASSERT(data->type == QInputDeviceManager::DeviceTypePointer);
    auto device = data->device;
    auto pointerData = data->get<PointerData>();
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
