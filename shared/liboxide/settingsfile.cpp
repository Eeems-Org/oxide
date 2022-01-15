#include <QFile>
#include <QDebug>

#include "settingsfile.h"

namespace Oxide {
    SettingsFile::SettingsFile(QString path)
        : QSettings(path, QSettings::IniFormat),
          fileWatcher(QStringList() << path)
    {

    }
    SettingsFile::~SettingsFile(){

    }
    void SettingsFile::fileChanged(){
        if(!fileWatcher.files().contains(fileName()) && !fileWatcher.addPath(fileName())){
            qWarning() << "Unable to watch " << fileName();
        }
        // Load new values
        sync();
        auto metaObj = metaObject();
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
    void SettingsFile::reloadProperty(const QString& name){
        auto metaObj = metaObject();

        auto propertyName = "__META_GROUP_" + name;
        auto idx = metaObj->indexOfProperty(propertyName.toStdString().c_str());
        if(idx == -1){
            O_SETTINGS_DEBUG("Group for " + name + " not found")
            return;
        }
        auto groupName = property(propertyName.toStdString().c_str()).toString();
        O_SETTINGS_DEBUG(fileName() + " Reloading " + groupName + "." + name)
        if(groupName != "General"){
            beginGroup(groupName);
        }
        if(contains(name)){
            O_SETTINGS_DEBUG("  Value exists")
            auto value = this->value(name);
            if(value.isValid()){
                O_SETTINGS_DEBUG("  Updated")
                setProperty(name.toStdString().c_str(), value);
            }else{
                O_SETTINGS_DEBUG("  Invalid value")
            }
        }else{
            O_SETTINGS_DEBUG("  No Value")
        }
        if(groupName != "General"){
            endGroup();
        }
        O_SETTINGS_DEBUG("  Done")
    }
    void SettingsFile::resetProperty(const QString& name){
        auto metaObj = metaObject();
        auto idx = metaObj->indexOfProperty(name.toStdString().c_str());
        if(idx == -1){
            O_SETTINGS_DEBUG(fileName() + " Property" + name + " not found")
            return;
        }
        auto property = metaObj->property(idx);
        if(property.isResettable()){
            property.reset(this);
        }else{
            reloadProperty(property.name());
        }
    }
    void SettingsFile::init(){
        if(initalized){
            return;
        }
        initalized = true;
        if(!QFile::exists(fileName())){
            resetProperties();
        }else{
            sync();
        }
        if(!fileWatcher.files().contains(fileName()) && !fileWatcher.addPath(fileName())){
            qWarning() << "Unable to watch " << fileName();
        }
        reloadProperties();
        connect(&fileWatcher, &QFileSystemWatcher::fileChanged, this, &SettingsFile::fileChanged);
    }
    void SettingsFile::reloadProperties(){
        auto metaObj = metaObject();
        for (int i = metaObj->propertyOffset(); i < metaObj->propertyCount(); ++i) {
            auto property = metaObj->property(i);
            if(property.isConstant()){
                continue;
            }
            reloadProperty(property.name());
        }
    }
    void SettingsFile::resetProperties(){
        auto metaObj = metaObject();
        for (int i = metaObj->propertyOffset(); i < metaObj->propertyCount(); ++i) {
            auto property = metaObj->property(i);
            if(property.isConstant()){
                continue;
            }
            resetProperty(property.name());
        }
    }
}
