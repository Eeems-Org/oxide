#include <QFile>
#include <QDebug>
#include <QMetaProperty>

#include "settingsfile.h"

namespace Oxide {
    SettingsFile::SettingsFile(QString path)
        : QSettings(path, QSettings::IniFormat),
          fileWatcher(QStringList() << path)
    {
        if(!QFile::exists(fileName())){
            auto metaObj = this->metaObject();
            for (int i = metaObj->propertyOffset(); i < metaObj->propertyCount(); ++i) {
                auto property = metaObj->property(i);
                if(property.isResettable()){
                    property.reset(this);
                }
            }
        }else{
            sync();
        }
        if(!fileWatcher.files().contains(fileName()) && !fileWatcher.addPath(fileName())){
            qWarning() << "Unable to watch " << fileName();
        }
        connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &SettingsFile::fileChanged);
    }
    SettingsFile::~SettingsFile(){

    }
    void SettingsFile::fileChanged(){
        if(!fileWatcher.files().contains(fileName()) && !fileWatcher.addPath(fileName())){
            qWarning() << "Unable to watch " << fileName();
        }
        // Load new values
        sync();
        auto metaObj = this->metaObject();
        for (int i = metaObj->propertyOffset(); i < metaObj->propertyCount(); ++i) {
            auto property = metaObj->property(i);
            if(property.hasNotifySignal()){
                auto value = property.read(this);
                auto value2 = this->value(property.name());
                if(value != value2){
                    property.write(this, value2);
                    property.notifySignal().invoke(this, Qt::BlockingQueuedConnection, QGenericArgument(value2.typeName(), value2.data()));
                }
            }
        }
    }
}
