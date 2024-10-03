#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QQmlApplicationEngine>
#include <QQmlContext>

namespace Oxide {
    namespace QML{
        class OxideQml : public QObject{
            Q_OBJECT
            Q_PROPERTY(bool landscape READ landscape NOTIFY landscapeChanged REVISION 1)
            QML_NAMED_ELEMENT(Oxide)
            QML_SINGLETON
        public:
            explicit OxideQml(QObject *parent = nullptr);
            bool landscape();

        signals:
            void landscapeChanged(bool);
        };
        OxideQml* getSingleton();
        void registerQML(QQmlApplicationEngine* engine);
    }
}
