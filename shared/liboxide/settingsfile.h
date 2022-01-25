#ifndef SETTINGSFILE_H
#define SETTINGSFILE_H

#include "liboxide_global.h"

#include <QSettings>
#include <QObject>
#include <QDebug>
#include <QFileSystemWatcher>
#include <QMetaProperty>
#include <QFile>

#include <cstring>

#ifdef DEBUG
#define O_SETTINGS_DEBUG(msg) if(debugEnabled()){ qDebug() << msg; }
#else
#define O_SETTINGS_DEBUG(msg)
#endif

#define O_SETTINGS_PROPERTY_0(_type, member, _group) \
    Q_PROPERTY(QString __META_GROUP_##member READ __META_GROUP_##member CONSTANT FINAL) \
    public: \
        void set_##member(_type _arg_##member); \
        _type member() const; \
        void reload_##member(); \
    Q_SIGNALS: \
        void member##Changed(const _type&); \
    protected: \
        QString __META_GROUP_##member() const; \
    private: \
        _type m_##member;

#define O_SETTINGS_PROPERTY_1(_type, group, member) \
    Q_PROPERTY(_type member MEMBER m_##member READ member WRITE set_##member NOTIFY member##Changed FINAL) \
    O_SETTINGS_PROPERTY_0(_type, member, group)

#define O_SETTINGS_PROPERTY_2(_type, group, member, _default) \
    Q_PROPERTY(_type member MEMBER m_##member READ member WRITE set_##member NOTIFY member##Changed RESET reset_##member) \
    O_SETTINGS_PROPERTY_0(_type, member, group) \
    public: \
        void reset_##member();

#define O_SETTINGS_PROPERTY_BODY_0(_class, _type, member, _group) \
    void _class::set_##member(_type _arg_##member) { \
        O_SETTINGS_DEBUG(fileName() + " Setting " + #_group + "." + #member) \
        m_##member = _arg_##member; \
        if(std::strcmp("General", #_group) != 0){ \
            beginGroup(#_group); \
        }else{ \
            beginGroup(""); \
        } \
        setValue(#member, QVariant::fromValue<_type>(_arg_##member)); \
        endGroup(); \
        sync(); \
    } \
    _type _class::member() const { return m_##member; } \
    void _class::reload_##member() { reloadProperty(#member); } \
    QString _class::__META_GROUP_##member() const { return #_group; }

#define O_SETTINGS_PROPERTY_BODY_1(_class, _type, group, member) \
    O_SETTINGS_PROPERTY_BODY_0(_class, _type, member, group)

#define O_SETTINGS_PROPERTY_BODY_2(_class, _type, group, member, _default) \
    O_SETTINGS_PROPERTY_BODY_0(_class, _type, member, group) \
    void _class::reset_##member() { \
        O_SETTINGS_DEBUG(fileName() + " Resetting " + #group + "." + #member) \
        O_SETTINGS_DEBUG(_default) \
        setProperty(#member, _default); \
        O_SETTINGS_DEBUG("  Done") \
    }

#define O_SETTINGS_PROPERTY_X_get_func(arg1, arg2, arg3, arg4, arg5, ...) arg5
#define O_SETTINGS_PROPERTY_X(...) \
    O_SETTINGS_PROPERTY_X_get_func(__VA_ARGS__, \
        O_SETTINGS_PROPERTY_2, \
        O_SETTINGS_PROPERTY_1, \
    )

#define O_SETTINGS_PROPERTY_BODY_X_get_func(arg1, arg2, arg3, arg4, arg5, arg6, ...) arg6
#define O_SETTINGS_PROPERTY_BODY_X(...) \
    O_SETTINGS_PROPERTY_BODY_X_get_func(__VA_ARGS__, \
        O_SETTINGS_PROPERTY_BODY_2, \
        O_SETTINGS_PROPERTY_BODY_1, \
    )

#define O_SETTINGS_PROPERTY(...) O_SETTINGS_PROPERTY_X(__VA_ARGS__)(__VA_ARGS__)
#define O_SETTINGS_PROPERTY_BODY(...) O_SETTINGS_PROPERTY_BODY_X(__VA_ARGS__)(__VA_ARGS__)


#define O_SETTINGS(_type, path) \
    public: \
        static _type& instance(){ \
            static _type INSTANCE(path); \
            INSTANCE.init(); \
            return INSTANCE; \
        } \
    private: \
    _type(QString _path) : SettingsFile(_path) {}


namespace Oxide {
    LIBOXIDE_EXPORT bool debugEnabled();
    class LIBOXIDE_EXPORT SettingsFile : public QSettings {
        Q_OBJECT
    private slots:
        void fileChanged();
    protected:
        SettingsFile(QString path);
        ~SettingsFile();
        void reloadProperty(const QString& name);
        void resetProperty(const QString& name);
        void init();
        void reloadProperties();
        void resetProperties();
    private:
        QFileSystemWatcher fileWatcher;
        bool initalized = false;
    };
}
#endif // SETTINGSFILE_H
