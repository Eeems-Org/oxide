#include <QFile>

#include "settingsfile.h"
#include "debug.h"

namespace Oxide {
    SettingsFile::SettingsFile(QString path)
        : QSettings(path, QSettings::IniFormat),
          reloadSemaphore(1),
          fileWatcher(QStringList() << path) { }
    SettingsFile::~SettingsFile(){ }
    void SettingsFile::fileChanged(){
        if(!fileWatcher.files().contains(fileName()) && !fileWatcher.addPath(fileName())){
            O_WARNING("Unable to watch " << fileName());
        }
        O_DEBUG("Settings file" << fileName() << "changed!");
        // Load new values
        sync();
        auto metaObj = metaObject();
        for (int i = metaObj->propertyOffset(); i < metaObj->propertyCount(); ++i) {
            auto prop = metaObj->property(i);
            if(QString(prop.name()).startsWith("__META_GROUP_") || !prop.isWritable() || !prop.hasNotifySignal()){
                continue;
            }
            auto value = prop.read(this);
            auto groupName = this->groupName(prop.name());
            if(groupName.isNull()){
                continue;
            }
            beginGroup(groupName != "General" ? groupName : "");
            bool exists = contains(prop.name());
            auto value2 = this->value(prop.name());
            endGroup();
            if(!exists){
                reloadSemaphore.acquire();
                if(prop.isResettable()){
                    prop.reset(this);
                }else if(!prop.read(this).isNull()){
                    prop.write(this, QVariant());
                }
                reloadSemaphore.release();
                continue;
            }
            if(!value2.isValid()){
                O_DEBUG("Property" << prop.name() << "new value invalid")
                continue;
            }
            if(value == value2){
                continue;
            }
            O_DEBUG("Property" << prop.name() << "changed" << value2)
            reloadSemaphore.acquire();
            prop.write(this, value2);
            reloadSemaphore.release();
        }
        O_DEBUG("Settings file" << fileName() << "changes loaded");
        emit changed();
    }
    void SettingsFile::reloadProperty(const QString& name){
        auto groupName = this->groupName(name);
        if(groupName.isNull()){
            return;
        }
        O_SETTINGS_DEBUG((fileName() + " Reloading " + groupName + "." + name).toStdString().c_str())
        reloadSemaphore.acquire();
        beginGroup(groupName != "General" ? groupName : "");
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
            auto metaObj = metaObject();
            auto prop = metaObj->property(metaObj->indexOfProperty(name.toStdString().c_str()));
            if(prop.isResettable()){
                resetProperty(name);
            }else if(!prop.read(this).isNull()){
                prop.write(this, QVariant());
            }
        }
        endGroup();
        reloadSemaphore.release();
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
        }
        sync();
        reloadProperties();
        if(!fileWatcher.files().contains(fileName()) && !fileWatcher.addPath(fileName())){
            O_WARNING("Unable to watch " << fileName());
        }
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
    QString SettingsFile::groupName(const QString& name){
        auto metaObj = metaObject();
        auto propertyName = "__META_GROUP_" + name;
        auto idx = metaObj->indexOfProperty(propertyName.toStdString().c_str());
        if(idx == -1){
            O_SETTINGS_DEBUG("Group for " + name + " not found")
            return QString();
        }
        return property(propertyName.toStdString().c_str()).toString();
    }
}

#include "moc_settingsfile.cpp"
