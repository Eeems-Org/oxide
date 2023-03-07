/*!
 * \addtogroup Oxide
 * @{
 * \file
 */
#pragma once

#include "liboxide_global.h"

#include <QSettings>
#include <QObject>
#include <QFileSystemWatcher>
#include <QMetaProperty>
#include <QFile>
#include <QSemaphore>

#include <cstring>

#define O_SETTINGS_DEBUG(msg) O_DEBUG(msg)

#define O_SETTINGS_PROPERTY_0(_type, member, _group) \
    Q_PROPERTY(QString __META_GROUP_##member READ __META_GROUP_##member CONSTANT FINAL) \
    public: \
        void set_##member(_type _arg_##member); \
        _type member() const; \
        bool has_##member(); \
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
        if(m_##member == _arg_##member){ \
            O_SETTINGS_DEBUG(fileName() + " No Change " + #_group + "." + #member) \
            return; \
        } \
        O_SETTINGS_DEBUG(fileName() + " Setting " + #_group + "." + #member) \
        m_##member = _arg_##member; \
        if(reloadSemaphore.tryAcquire()){ \
            beginGroup(std::strcmp("General", #_group) != 0 ? #_group : ""); \
            setValue(#member, QVariant::fromValue<_type>(_arg_##member)); \
            endGroup(); \
            O_SETTINGS_DEBUG(fileName() + " Saving " + #_group + "." + #member) \
            sync(); \
            reloadSemaphore.release(); \
        }else{ \
            O_SETTINGS_DEBUG(fileName() + " Not Saving " + #_group + "." + #member) \
        } \
        emit member##Changed(m_##member);\
    } \
    _type _class::member() const { return m_##member; } \
    bool _class::has_##member() { \
        beginGroup(std::strcmp("General", #_group) != 0 ? #_group : ""); \
        bool res = contains(#member); \
        endGroup(); \
        return res; \
    } \
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
/*!
 * \def O_SETTINGS_PROPERTY
 * \brief Add a property to a SettingsFile derived class
 * \param _type Type of property
 * \param group Group name for property. This usually should be ``General``
 * \param member Property name
 * \param _default Optional default value
 * \sa  O_SETTINGS, O_SETTINGS_PROPERTY_BODY, Oxide::SettingsFile
 */
#define O_SETTINGS_PROPERTY(...) O_SETTINGS_PROPERTY_X(__VA_ARGS__)(__VA_ARGS__)
/*!
 * \def O_SETTINGS_PROPERTY_BODY
 * \brief Add the body for a property on a SettingsFile derived class
 * \param _class Class name
 * \param _type Type of property
 * \param group Group name for property. This usually should be ``General``
 * \param member Property name
 * \param _default Optional default value
 * \sa  O_SETTINGS, O_SETTINGS_PROPERTY, Oxide::SettingsFile
 */
#define O_SETTINGS_PROPERTY_BODY(...) O_SETTINGS_PROPERTY_BODY_X(__VA_ARGS__)(__VA_ARGS__)

/*!
 * \def O_SETTINGS
 * \brief Define the instance() and constructor methods for a SettingsFile derived class
 * \param _type Class name
 * \param path Path to file on disk that stores the settings
 * \sa  O_SETTINGS_PROPERTY, O_SETTINGS_PROPERTY_BODY, Oxide::SettingsFile
 */
#define O_SETTINGS(_type, path) \
    public: \
        static _type& instance(){ \
            static _type INSTANCE(path); \
            INSTANCE.init(); \
            return INSTANCE; \
        } \
    private: \
    explicit _type(const QString& _path) : SettingsFile(_path) {}


namespace Oxide {
    /*!
     * \brief A better version of [QSettings](https://doc.qt.io/qt-5/qsettings.html)
     *
     * This base class adds dynamic updates of changes to a settings file from disk.
     * It also implements a static instance method that will return the singleton for this class.
     *
     * \sa sharedSettings, xochitlSettings, O_SETTINGS, O_SETTINGS_PROPERTY, O_SETTINGS_PROPERTY_BODY
     */
    class LIBOXIDE_EXPORT SettingsFile : public QSettings {
        Q_OBJECT
    signals:
        void changed();

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
        QString groupName(const QString& name);
        QSemaphore reloadSemaphore;

    private:
        QFileSystemWatcher fileWatcher;
        bool initalized = false;
    };
}
/*! @} */
