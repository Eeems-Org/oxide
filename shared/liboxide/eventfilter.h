/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <QObject>
#include <QEvent>
#include <QQuickItem>
namespace Oxide{
    /*!
     * \brief An event filter that maps pen events to Qt touch events.
     *
     * It works by handling tablet events and translating them to mouse events.
     * They are then sent to every enabled/visible widget at the x/y coordinate that have a parent widget, and accept left mouse button input.
     * This doens't always work as some widgets aren't found with the current method of finding widgets at a location.
     *
     * The following is an example of adding it to an application:
     * \snippet examples/oxide.cpp EventFilter
     */
    class LIBOXIDE_EXPORT EventFilter : public QObject
    {
        Q_OBJECT
    public:
        /*!
         * \brief The root element in the Qt application
         *
         * This is needed to find wigets to send mouse events when a pen touch is detected
         */
        QQuickItem* root;
        /*!
         * \brief Create a new EventFilter instance
         * \param parent The parent object. Usually should be qApp
         */
        explicit EventFilter(QObject* parent = nullptr);
    signals:
        void suspend();
    protected:
        bool eventFilter(QObject* obj, QEvent* ev);
    };
}
/*! @} */
