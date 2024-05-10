#include "oxideqml.h"
#include "liboxide.h"

namespace Oxide {
    namespace QML{
        OxideQml::OxideQml(QObject *parent) : QObject{parent}{
            deviceSettings.onKeyboardAttachedChanged([this]{
                emit landscapeChanged(deviceSettings.keyboardAttached());
            });
        }

        bool OxideQml::landscape(){ return deviceSettings.keyboardAttached(); }

        OxideQml* getSingleton(){
            static OxideQml* instance = new OxideQml(qApp);
            return instance;
        }

        void registerQML(QQmlApplicationEngine* engine){
            QQmlContext* context = engine->rootContext();
            context->setContextProperty("Oxide", getSingleton());
            engine->addImportPath( "qrc:/codes.eeems.oxide" );
        }
    }
}
