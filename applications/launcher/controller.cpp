#include <QIODevice>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QSet>
#include <QDebug>

#include <unistd.h>
#include <sstream>
#include <memory>
#include <fstream>
#include <liboxide.h>

#include "controller.h"

QSet<QString> settings = { "columns", "autoStartApplication" };
QSet<QString> booleanSettings {"showWifiDb", "showBatteryPercent", "showBatteryTemperature", "showDate" };
QList<QString> configDirectoryPaths = { "/opt/etc/draft", "/etc/draft", "/home/root/.config/draft" };
QList<QString> configFileDirectoryPaths = { "/opt/etc", "/etc", "/home/root/.config" };

QFile* getConfigFile(){
    for(auto path : configFileDirectoryPaths){
        QDir dir(path);
        if(dir.exists() && !dir.isEmpty()){
            QFile* file = new QFile(path + "/oxide.conf");
            if(file->exists()){
                return file;
            }
        }
    }
    return nullptr;
}
bool configFileExists(){
    auto configFile = getConfigFile();
    return configFile != nullptr && configFile->exists();
}
void Controller::loadSettings(){
    // If the config directory doesn't exist,
    // then print an error and stop.
    qDebug() << "parsing config file...";
    if(configFileExists()){
        auto configFile = getConfigFile();
        if (!configFile->open(QIODevice::ReadOnly | QIODevice::Text)) {
            qCritical() << "Couldn't read the config file. " << configFile->fileName();
            return;
        }
        QTextStream in(configFile);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if(!line.startsWith("#") && !line.isEmpty()){
                QStringList parts = line.split("=");
                if(parts.length() != 2){
                    O_WARNING("  Wrong format on " << line);
                    continue;
                }
                QString lhs = parts.at(0).trimmed();
                QString rhs = parts.at(1).trimmed();
                if (booleanSettings.contains(lhs)){
                    auto property = lhs.toStdString();
                    auto value = rhs.toLower();
                    auto boolValue = value == "true" || value == "t" || value == "y" || value == "yes" || value == "1";
                    setProperty(property.c_str(), boolValue);
                    qDebug() << "  " << (property + ":").c_str() << boolValue;
                }else if(settings.contains(lhs)){
                    auto property = lhs.toStdString();
                    setProperty(property.c_str(), rhs);
                    qDebug() << "  " << (property + ":").c_str() << rhs.toStdString().c_str();
                }
            }
        }
        configFile->close();
    }
    qDebug() << "Finished parsing config file.";
    auto sleepAfter = systemApi->autoSleep();
    qDebug() << "Automatic sleep" << sleepAfter;
    setAutomaticSleep(sleepAfter);
    setSleepAfter(sleepAfter);
    auto lockOnSuspend = systemApi->lockOnSuspend();
    qDebug() << "Lock on suspend" << lockOnSuspend;
    setLockOnSuspend(lockOnSuspend);
    auto autoLock = systemApi->autoLock();
    qDebug() << "Automatic lock" << autoLock;
    setAutomaticLock(autoLock);
    setLockAfter(autoLock);
    for(short i = 1; i <= 4; i++){
        setSwipeLength(i, systemApi->getSwipeLength(i));
    }
    qDebug() << "Updating UI with settings from config file...";
    // Populate settings in UI
    QObject* columnsSpinBox = root->findChild<QObject*>("columnsSpinBox");
    if(!columnsSpinBox){
        qDebug() << "Can't find columnsSpinBox";
    }else{
        columnsSpinBox->setProperty("value", columns());
    }
    QObject* sleepAfterSpinBox = root->findChild<QObject*>("sleepAfterSpinBox");
    if(!sleepAfterSpinBox){
        qDebug() << "Can't find sleepAfterSpinBox";
    }else{
        sleepAfterSpinBox->setProperty("value", this->sleepAfter());
    }
    QObject* swipeLengthRightSpinBox = root->findChild<QObject*>("swipeLengthRightSpinBox");
    if(!swipeLengthRightSpinBox){
        qDebug() << "Can't find swipeLengthRightSpinBox";
    }else{
        swipeLengthRightSpinBox->setProperty("value", this->swipeLengthRight());
    }
    QObject* swipeLengthLeftSpinBox = root->findChild<QObject*>("swipeLengthLeftSpinBox");
    if(!swipeLengthLeftSpinBox){
        qDebug() << "Can't find swipeLengthLeftSpinBox";
    }else{
        swipeLengthLeftSpinBox->setProperty("value", this->swipeLengthLeft());
    }
    QObject* swipeLengthUpSpinBox = root->findChild<QObject*>("swipeLengthUpSpinBox");
    if(!swipeLengthUpSpinBox){
        qDebug() << "Can't find swipeLengthUpSpinBox";
    }else{
        swipeLengthUpSpinBox->setProperty("value", this->swipeLengthUp());
    }
    QObject* swipeLengthDownSpinBox = root->findChild<QObject*>("swipeLengthDownSpinBox");
    if(!swipeLengthDownSpinBox){
        qDebug() << "Can't find swipeLengthDownSpinBox";
    }else{
        swipeLengthDownSpinBox->setProperty("value", this->swipeLengthDown());
    }
    qDebug() << "Finished updating UI.";
}
void Controller::saveSettings(){
    qDebug() << "Saving configuration...";
    QSet<QString> items = settings;
    items.detach();
    QSet<QString> booleanItems = booleanSettings;
    booleanItems.detach();
    std::stringstream buffer;
    auto configFile = getConfigFile();
    if(configFile == nullptr){
        configFile = new QFile(configFileDirectoryPaths.last() + "/oxide.conf");
    }
    QTextStream stream(configFile);
    if(configFile->exists()){
        if (!configFile->open(QIODevice::ReadWrite | QIODevice::Text)) {
            qCritical() << "Unable to edit the config file. " << configFile->fileName();
            return;
        }
        while (!stream.atEnd()) {
            QString line = stream.readLine();
            if(line.startsWith("#")){
                buffer << line.toStdString() << std::endl;
                continue;
            }
            QStringList parts = line.split("=");
            if(parts.length() != 2){
                buffer << line.toStdString() << std::endl;
                continue;
            }
            QString lhs = parts.at(0);
            QString rhs = parts.at(1);
            auto propertyName = lhs.toStdString();
            if (booleanItems.contains(lhs)){
                if(property(propertyName.c_str()).toBool()){
                    rhs = "yes";
                }else{
                    rhs = "no";
                }
                items.remove(lhs);
            }else if(items.contains(lhs)){
                rhs = property(propertyName.c_str()).toString();
                items.remove(lhs);
            }
            auto value = rhs.toStdString();
            qDebug() <<  " " << (propertyName + ":").c_str() << value.c_str();
            buffer << propertyName << "=" << value << std::endl;
        }
    }else if (!configFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "Unable to create the config file. " << configFile->fileName();
        return;
    }
    for(QString item : items){
        auto propertyName = item.toStdString();
        auto value = property(item.toStdString().c_str()).toString().toStdString();
        qDebug() <<  " " << (propertyName + ":").c_str() << value.c_str();
        buffer << propertyName << "=" << value << std::endl;
    }
    configFile->resize(0);
    stream << QString::fromStdString(buffer.str());
    configFile->close();
    if(!m_automaticSleep){
        systemApi->setAutoSleep(0);
    }else{
        systemApi->setAutoSleep(m_sleepAfter);
        auto sleepAfter = systemApi->autoSleep();
        if(sleepAfter != m_sleepAfter){
            setSleepAfter(sleepAfter);
            setAutomaticSleep(sleepAfter);
        }
    }
    systemApi->setLockOnSuspend(m_lockOnSuspend);
    if(!m_automaticLock){
        systemApi->setAutoLock(0);
    }else{
        systemApi->setAutoLock(m_lockAfter);
        auto lockAfter = systemApi->autoLock();
        if(lockAfter != m_lockAfter){
            setLockAfter(lockAfter);
            setAutomaticLock(lockAfter);
        }
    }
    for(short i = 1; i <= 4; i++) {
        systemApi->setSwipeLength(i, getSwipeLength(i));
    }
    qDebug() << "Done saving configuration.";
}
QList<QObject*> Controller::getApps(){
    auto bus = QDBusConnection::systemBus();
    auto running = appsApi->runningApplications();
    auto paused = appsApi->pausedApplications();
    for(auto key : paused.keys()){
        if(running.contains(key)){
            continue;
        }
        running.insert(key, paused[key]);
    }
    for(auto item : appsApi->applications()){
        auto path = item.value<QDBusObjectPath>().path();
        Application app(OXIDE_SERVICE, path, bus, this);
        if(app.hidden() || app.transient()){
            continue;
        }
        auto name = app.name();
        auto appItem = getApplication(name);
        if(appItem == nullptr){
            qDebug() << "New application:" << name;
            appItem = new AppItem(this);
            applications.append(appItem);
        }
        auto displayName = app.displayName();
        if(displayName.isEmpty()){
            displayName = name;
        }
        appItem->setProperty("path", path);
        appItem->setProperty("name", name);
        appItem->setProperty("displayName", displayName);
        appItem->setProperty("desc", app.description());
        appItem->setProperty("call", app.bin());
        appItem->setProperty("running", running.contains(name));
        auto icon = app.icon();
        if(!icon.isEmpty() && QFile(icon).exists()){
            appItem->setProperty("imgFile", "file:" + icon);
        }
        if(!appItem->ok()){
            qDebug() << "Invalid item" << appItem->property("name").toString();
            applications.removeAll(appItem);
            appItem->deleteLater();
        }
    }
    // Sort by name
    std::sort(applications.begin(), applications.end(), [=](const QObject* a, const QObject* b) -> bool {
        return a->property("displayName").toString() < b->property("displayName").toString();
    });
    return applications;
}
void Controller::setAutoStartApplication(QString autoStartApplication){
    m_autoStartApplication = autoStartApplication;
    emit autoStartApplicationChanged(autoStartApplication);
}
AppItem* Controller::getApplication(QString name){
    for(auto app : applications){
        if(app->property("name").toString() == name){
            return reinterpret_cast<AppItem*>(app);
        }
    }
    return nullptr;
}

void Controller::importDraftApps(){ QProcess::execute("update-desktop-database", QStringList() << "--verbose"); }
void Controller::powerOff(){
    qDebug() << "Powering off...";
    systemApi->powerOff();
}
void Controller::reboot(){
    qDebug() << "Rebooting...";
    systemApi->reboot();
}
void Controller::suspend(){
    qDebug() << "Suspending...";
    systemApi->suspend();
}

void Controller::lock(){
    qDebug() << "Locking...";
    appsApi->openLockScreen();
}
inline void updateUI(QObject* ui, const char* name, const QVariant& value){
    if(ui){
        ui->setProperty(name, value);
    }
}
std::string Controller::exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}
void Controller::updateUIElements(){}
void Controller::setAutomaticSleep(bool state){
    m_automaticSleep = state;
    if(state){
        qDebug() << "Enabling automatic sleep";
    }else{
        qDebug() << "Disabling automatic sleep";
    }
    emit automaticSleepChanged(state);
}
void Controller::setLockOnSuspend(bool state){
    m_lockOnSuspend = state;
    if(state){
        qDebug() << "Enabling lock on suspend";
    }else{
        qDebug() << "Disabling lock on suspend";
    }
    emit lockOnSuspendChanged(state);
}
void Controller::setAutomaticLock(bool state){
    m_automaticLock = state;
    if(state){
        qDebug() << "Enabling automatic lock";
    }else{
        qDebug() << "Disabling automatic lock";
    }
    emit automaticLockChanged(state);
}
void Controller::setColumns(int columns){
    m_columns = columns;
    if(root != nullptr){
        qDebug() << "Columns: " << columns;
        emit columnsChanged(columns);
    }
}
void Controller::setSwipeLength(int direction, int length){
    switch (direction){
        case SwipeDirection::Right:
            m_swipeLengthRight = length;
            emit swipeLengthRightChanged(length);
            break;
        case SwipeDirection::Left:
            m_swipeLengthLeft = length;
            emit swipeLengthLeftChanged(length);
            break;
        case SwipeDirection::Up:
            m_swipeLengthUp = length;
            emit swipeLengthUpChanged(length);
            break;
        case SwipeDirection::Down:
            m_swipeLengthDown = length;
            emit swipeLengthDownChanged(length);
            break;
        default:
            qDebug() << "Invalid swipe direction: " << direction;
            break;
    }
}
int Controller::getSwipeLength(int direction){
    switch (direction){
        case SwipeDirection::Right:
            return swipeLengthRight();
        case SwipeDirection::Left:
            return swipeLengthLeft();
        case SwipeDirection::Up:
            return swipeLengthUp();
        case SwipeDirection::Down:
            return swipeLengthDown();
        default:
            qDebug() << "Invalid swipe direction: " << direction;
            return -1;
    }
}
void Controller::setShowWifiDb(bool state){
    m_showWifiDb = state;
    if(root != nullptr){
        qDebug() << "Show Wifi DB: " << state;
        emit showWifiDbChanged(state);
    }
    if(state){
        updateUIElements();
    }
}
void Controller::setShowBatteryPercent(bool state){
    m_showBatteryPercent = state;
    if(root != nullptr){
        qDebug() << "Show Battery Percentage: " << state;
        emit showBatteryPercentChanged(state);
    }
}
void Controller::setShowBatteryTemperature(bool state){
    m_showBatteryTemperature = state;
    if(root != nullptr){
        qDebug() << "Show Battery Temperature: " << state;
        emit showBatteryTemperatureChanged(state);
    }
}
void Controller::setSleepAfter(int sleepAfter){
    m_sleepAfter = sleepAfter;
    qDebug() << "Sleep After: " << sleepAfter << " minutes";
    emit sleepAfterChanged(m_sleepAfter);
}
void Controller::setLockAfter(int lockAfter){
    m_lockAfter = lockAfter;
    qDebug() << "Lock After: " << lockAfter << " minutes";
    emit lockAfterChanged(m_lockAfter);
}

#include "moc_controller.cpp"
