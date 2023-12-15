#include "liboxide.h"

#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QTimer>

#include <pwd.h>
#include <grp.h>
#include <linux/input.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>

namespace Oxide {
    // https://stackoverflow.com/a/1643134
    int tryGetLock(char const* lockName){
        mode_t m = umask(0);
        int fd = open(lockName, O_RDWR | O_CREAT, 0666);
        umask(m);
        if(fd < 0){
            return -1;
        }
        if(!flock(fd, LOCK_EX | LOCK_NB)){
            return fd;
        }
        close(fd);
        return -1;
    }
    void releaseLock(int fd, char const* lockName){
        if(fd < 0){
            return;
        }
        if(!flock(fd, F_ULOCK | LOCK_NB)){
            remove(lockName);
        }
        close(fd);
    }
    bool processExists(pid_t pid){ return QFile::exists(QString("/proc/%1").arg(pid)); }
    QList<pid_t> lsof(const QString& path){
        QList<pid_t> pids;
        QDir directory("/proc");
        if (!directory.exists() || directory.isEmpty()){
            qCritical() << "Unable to access /proc";
            return pids;
        }
        QString qpath(QFileInfo(path).canonicalFilePath());
        auto processes = directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name);
        // Get all pids we care about
        for(QFileInfo fi : processes){
            auto pid = fi.baseName().toUInt();
            if(!pid || !processExists(pid)){
                continue;
            }
            QFile statm(QString("/proc/%1/statm").arg(pid));
            QTextStream stream(&statm);
            if(!statm.open(QIODevice::ReadOnly | QIODevice::Text)){
                continue;
            }
            QString content = stream.readAll().trimmed();
            statm.close();
            // Ignore kernel processes
            if(content == "0 0 0 0 0 0 0"){
                continue;
            }
            QDir fd_directory(QString("/proc/%1/fd").arg(pid));
            if(!fd_directory.exists() || fd_directory.isEmpty()){
                continue;
            }
            auto fds = fd_directory.entryInfoList(QDir::Files | QDir::NoDot | QDir::NoDotDot);
            for(QFileInfo fd : fds){
                if(fd.canonicalFilePath() == qpath){
                    pids.append(pid);
                }
            }
        }
        return pids;
    }
    uid_t getUID(const QString& name){
        char buffer[1024];
        struct passwd user;
        struct passwd* result;
        auto status = getpwnam_r(name.toStdString().c_str(), &user, buffer, sizeof(buffer), &result);
        if(status != 0){
            throw std::runtime_error("Failed to get user" + status);
        }
        if(result == NULL){
            throw std::runtime_error("Invalid user name: " + name.toStdString());
        }
        return result->pw_uid;
    }
    gid_t getGID(const QString& name){
        char buffer[1024];
        struct group grp;
        struct group* result;
        auto status = getgrnam_r(name.toStdString().c_str(), &grp, buffer, sizeof(buffer), &result);
        if(status != 0){
            throw std::runtime_error("Failed to get group" + status);
        }
        if(result == NULL){
            throw std::runtime_error("Invalid group name: " + name.toStdString());
        }
        return result->gr_gid;
    }

    WifiNetworks XochitlSettings::wifinetworks(){
        beginGroup("wifinetworks");
        QMap<QString, QVariantMap> wifinetworks;
        for(const QString& key : allKeys()){
            QVariantMap network = value(key).toMap();
            wifinetworks[key] = network;
        }
        endGroup();
        return wifinetworks;
    }
    void XochitlSettings::setWifinetworks(const WifiNetworks& wifinetworks){
        beginGroup("wifinetworks");
        for(const QString& key : wifinetworks.keys()){
            setValue(key, wifinetworks.value(key));
        }
        endGroup();
        sync();
    }
    QVariantMap XochitlSettings::getWifiNetwork(const QString& name){
        beginGroup("wifinetworks");
        QVariantMap network = value(name).toMap();
        endGroup();
        return network;
    }
    void XochitlSettings::setWifiNetwork(const QString& name, QVariantMap properties){
        beginGroup("wifinetworks");
        setValue(name, properties);
        endGroup();
        sync();
    }
    void XochitlSettings::resetWifinetworks(){}
    XochitlSettings::~XochitlSettings(){}
    SharedSettings::~SharedSettings(){}
    O_SETTINGS_PROPERTY_BODY(XochitlSettings, QString, General, passcode)
    O_SETTINGS_PROPERTY_BODY(XochitlSettings, bool, General, wifion)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, int, General, version)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, firstLaunch, true)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, telemetry, false)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, applicationUsage, false)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, crashReport, true)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, bool, General, lockOnSuspend, true)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, int, General, autoSleep, 5)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, int, General, autoLock, 5)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, QString, Lockscreen, pin)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, QString, Lockscreen, onLogin)
    O_SETTINGS_PROPERTY_BODY(SharedSettings, QString, Lockscreen, onFailedLogin)
}

#include "moc_liboxide.cpp"
