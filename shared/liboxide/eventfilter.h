/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once

#include <QObject>
#include <QEvent>
#include <QQuickItem>
namespace Oxide{
    /*!
     * \brief An event filter that maps pen events to Qt touch events
     */
    class EventFilter : public QObject
    {
        Q_OBJECT
    public:
        /*!
         * \brief The root element in the Qt application
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
