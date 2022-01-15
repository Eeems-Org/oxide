#ifndef SETTINGSFILE_H
#define SETTINGSFILE_H

#include <QSettings>
#include <QObject>
#include <QFileSystemWatcher>

#define O_SETTINGS_PROPERTY_0(_type, member, group) \
    public: \
      void set_##member(_type _arg_##member) { \
          if(m_##member != _arg_##member) { \
            m_##member = _arg_##member; \
            beginGroup(#group); \
            setValue(#member, QVariant::fromValue<_type>(_arg_##member)); \
            endGroup(); \
            sync(); \
          } \
        } \
        _type member() const { return m_##member; } \
    Q_SIGNALS: \
        void member##Changed(const _type&); \
    private: \
        _type m_##member;

#define O_SETTINGS_PROPERTY_1(_type, group, member) \
    Q_PROPERTY(_type member MEMBER m_##member READ member WRITE set_##member NOTIFY member##Changed) \
    O_SETTINGS_PROPERTY_0(_type, member, group)

#define O_SETTINGS_PROPERTY_2(_type, group, member, _default) \
    Q_PROPERTY(_type member MEMBER m_##member READ member WRITE set_##member NOTIFY member##Changed RESET reset_##member) \
    O_SETTINGS_PROPERTY_0(_type, member, group) \
    public: \
        void reset_##member() { m_##member = _default; }

#define O_SETTINGS_PROPERTY_X_get_func(arg1, arg2, arg3, arg4, arg5, ...) arg5
#define O_SETTINGS_PROPERTY_X(...) \
    O_SETTINGS_PROPERTY_X_get_func(__VA_ARGS__, \
        O_SETTINGS_PROPERTY_2, \
        O_SETTINGS_PROPERTY_1, \
    )

#define O_SETTINGS_PROPERTY(...) O_SETTINGS_PROPERTY_X(__VA_ARGS__)(__VA_ARGS__)


#define O_SETTINGS(_type, path) \
    public: \
        static _type& instance(){ \
            static _type INSTANCE(path); \
            return INSTANCE; \
        } \
    private: \
    _type(QString _path) : SettingsFile(_path) {}


namespace Oxide {
    class SettingsFile : public QSettings {
        Q_OBJECT
    private slots:
        void fileChanged();
    protected:
        SettingsFile(QString path);
        ~SettingsFile();
    private:
        QFileSystemWatcher fileWatcher;
    };
}
#endif // SETTINGSFILE_H
