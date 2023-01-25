/*!
 * \file slothandler.h
 */
#ifndef SLOTHANDLER_H
#define SLOTHANDLER_H

#include "liboxide_global.h"

#include <QObject>
#include <QDBusServiceWatcher>
#include <QDBusAbstractInterface>

namespace Oxide{
    /*!
     * \brief Connect to a slot on a DBus interface
     * \param Interface to connect to
     * \param Slot to connect to
     * \param Method to run when an event is recieved on the slot
     * \param If this should disconnect after the first event
     * \return If the connect succeeded
     */
    LIBOXIDE_EXPORT bool DBusConnect(QDBusAbstractInterface* interface, const QString& slotName, std::function<void(QVariantList)> onMessage, const bool& once=false);
    /*!
     * \brief Connect to a slot on a DBus interface
     * \param Interface to connect to
     * \param Slot to connect to
     * \param Method to run when an event is recieved on the slot
     * \param Method to run when disconnecting
     * \param If this should disconnect after the first event
     * \return If the connect succeeded
     */
    LIBOXIDE_EXPORT bool DBusConnect(QDBusAbstractInterface* interface, const QString& slotName, std::function<void(QVariantList)> onMessage, std::function<void()> callback, const bool& once=false);
    /*!
     * \brief A class for handling DBus slots
     */
    class LIBOXIDE_EXPORT SlotHandler : public QObject {
    public:
        SlotHandler(const QString& serviceName, QStringList parameters, bool once, std::function<void(QVariantList)> onMessage, std::function<void()> callback);
        ~SlotHandler();
        int qt_metacall(QMetaObject::Call call, int id, void **arguments);
        bool connect(QObject* sender, int methodId);

    private:
        QString serviceName;
        QStringList parameters;
        bool once;
        bool m_disconnected;
        QDBusServiceWatcher* watcher;
        std::function<void(QVariantList)> onMessage;
        std::function<void()> callback;
        void handleSlot(QObject* api, void** arguments);
    };
}

#endif // SLOTHANDLER_H
