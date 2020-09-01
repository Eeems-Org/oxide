#include "controller.h"
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
#include "sysobject.h"

QSet<QString> settings = { "columns", "fontSize", "sleepAfter" };
QSet<QString> booleanSettings {"automaticSleep", "showWifiDb", "showBatteryPercentage" };
QList<QString> configDirectoryPaths = { "/opt/etc/draft", "/etc/draft", "/home/root /.config/draft" };


QFile* getConfigFile(){
    for(auto path : configDirectoryPaths){
        QDir dir(path);
        if(dir.exists() && !dir.isEmpty()){
            QFile* file = new QFile(path + "/conf");
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
    qDebug() << "parsing config file ";
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
                    qWarning() << "wrong format on " << line;
                    continue;
                }
                QString lhs = parts.at(0);
                QString rhs = parts.at(1);
                if (booleanSettings.contains(lhs)){
                    auto value = rhs.toLower();
                    setProperty(lhs.toStdString().c_str(), value == "true" || value == "t" || value == "y" || value == "yes" || value == "1");
                }else if(settings.contains(lhs)){
                    setProperty(lhs.toStdString().c_str(), rhs);
                }
            }
        }
        configFile->close();
    }
    if(root){
        // Populate settings in UI
        QObject* columnsSpinBox = root->findChild<QObject*>("columnsSpinBox");
        if(!columnsSpinBox){
            qDebug() << "Can't find columnsSpinBox";
        }else{
            columnsSpinBox->setProperty("value", columns());
        }
        QObject* fontSizeSpinBox = root->findChild<QObject*>("fontSizeSpinBox");
        if(!fontSizeSpinBox){
            qDebug() << "Can't find fontSizeSpinBox";
        }else{
            fontSizeSpinBox->setProperty("value", fontSize());
        }
        QObject* sleepAfterSpinBox = root->findChild<QObject*>("sleepAfterSpinBox");
        if(!sleepAfterSpinBox){
            qDebug() << "Can't find sleepAfterSpinBox";
        }else{
            sleepAfterSpinBox->setProperty("value", sleepAfter());
        }
    }
}
void Controller::saveSettings(){
    qDebug() << "Saving configuration";
    QSet<QString> items = settings;
    items.detach();
    QSet<QString> booleanItems = booleanSettings;
    booleanItems.detach();
    std::stringstream buffer;
    auto configFile = getConfigFile();
    if(configFile == nullptr){
        configFile = new QFile(configDirectoryPaths.last() + "/conf");
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
            if (booleanItems.contains(lhs)){
                if(property(lhs.toStdString().c_str()).toBool()){
                    rhs = "yes";
                }else{
                    rhs = "no";
                }
                items.remove(lhs);
            }else if(items.contains(lhs)){
                rhs = property(lhs.toStdString().c_str()).toString();
                items.remove(lhs);
            }
            buffer << lhs.toStdString() << "=" << rhs.toStdString() << std::endl;
        }
    }else if (!configFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "Unable to create the config file. " << configFile->fileName();
        return;
    }
    for(QString item : items){
        buffer << item.toStdString() << "=" << property(item.toStdString().c_str()).toString().toStdString() << std::endl;
    }
    configFile->resize(0);
    stream << QString::fromStdString(buffer.str());
    configFile->close();
}
QList<QObject*> Controller::getApps(){
    QList<QObject*> result;
    // If the config directory doesn't exist,
    // then print an error and stop.
    for(auto configDirectoryPath : configDirectoryPaths){
        QDir configDirectory(configDirectoryPath);
        configDirectory.setFilter( QDir::Files | QDir::NoSymLinks | QDir::NoDot | QDir::NoDotDot);
        auto images = configDirectory.entryInfoList(QDir::NoFilter,QDir::SortFlag::Name);
        foreach(QFileInfo fi, images){
            if(fi.fileName() != "conf"){
                auto f = fi.absoluteFilePath();

                qDebug() << "parsing file " << f;

                QFile file(fi.absoluteFilePath());
                if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    qCritical() << "Couldn't find the file " << f;
                    continue;
                }

                QTextStream in(&file);

                AppItem* app = new AppItem();

                while (!in.atEnd()) {
                    QString line = in.readLine();
                    QStringList parts = line.split("=");
                    if(parts.length() != 2){
                        qWarning() << "wrong format on " << line;
                        continue;
                    }
                    QString lhs = parts.at(0);
                    QString rhs = parts.at(1);
                    QSet<QString> known = { "name", "desc", "call", "term" };
                    if (known.contains(lhs)){
                        app->setProperty(lhs.toUtf8(), rhs);
                    }else if (lhs == "imgFile"){
                        if(rhs != ":" && rhs != ""){
                            app->setProperty(lhs.toUtf8(), "file:" + configDirectoryPath + "/icons/" + rhs + ".png");
                        }else{
                            app->setProperty(lhs.toUtf8(), "qrc:/img/icon.png");
                        }
                    }
                }
                if(app ->ok()){
                    result.append(app );
                }
                file.close();
            }
        }
    }
    return result;
}
void Controller::powerOff(){
    qDebug() << "Powering off...";
    system("systemctl poweroff");
}
void Controller::suspend(){
    qDebug() << "Suspending...";
    system("systemctl suspend");
}
void Controller::killXochitl(){
    if(system("systemctl is-active --quiet xochitl")){
        qDebug() << "Killing xochtil";
        system("systemctl stop xochtil");
    }
}
void Controller::updateBatteryLevel() {
    QObject* ui = root->findChild<QObject*>("batteryLevel");
    if(!ui){
        qDebug() << "Can't find batteryLevel";
    }
    if(!battery.exists()){
        if(!batteryWarning){
            qWarning() << "Can't find battery information";
            batteryWarning = true;
            if(ui){
                ui->setProperty("warning", true);
            }
        }
        return;
    }
    if(!battery.intProperty("present")){
        qWarning() << "Battery is somehow not in the device?";
        if(!batteryWarning){
            qWarning() << "Can't find battery information";
            batteryWarning = true;
            if(ui){
                ui->setProperty("warning", true);
            }
        }
        return;
    }
    int battery_level = battery.intProperty("capacity");
    if(batteryLevel != battery_level){
        batteryLevel = battery_level;
        if(ui){
            ui->setProperty("level", batteryLevel);
        }
    }
    std::string status = battery.strProperty("status");
    auto charging = status == "Charging";
    if(batteryCharging != charging){
        batteryCharging = charging;
        if(ui){
            ui->setProperty("charging", charging);
        }
    }
    std::string capacityLevel = battery.strProperty("capacity_level");
    auto alert = capacityLevel == "Critical" || capacityLevel == "";
    if(batteryAlert != alert){
        batteryAlert = alert;
        if(ui){
            ui->setProperty("alert", alert);
        }
    }
    auto warning = status == "Unknown" || status == "" || capacityLevel == "Unknown";
    if(batteryWarning != warning){
        batteryWarning = warning;
        if(ui){
            ui->setProperty("warning", warning);
        }
    }
    if(showBatteryTemperature()){
        int temperature = battery.intProperty("temp") / 10;
        if(batteryTemperature != temperature){
            batteryTemperature = temperature;
            if(ui){
                ui->setProperty("temperature", temperature);
            }
        }
    }
    return;
}
std::string exec(const char* cmd) {
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
void Controller::updateWifiState(){
    QObject* ui = root->findChild<QObject*>("wifiState");
    if(!ui){
        qDebug() << "Can't find wifiState";
    }
    if(!wifi.exists()){
        if(wifiState != "unknown"){
            qDebug() << "Unable to get wifi information";
            wifiState = "unknown";
            if(ui){
                ui->setProperty("state", "unkown");
            }
        }
        return;
    }
    auto state = wifi.strProperty("operstate");
    if(wifiState.toStdString() != state){
        wifiState = state.c_str();
        if(ui){
            ui->setProperty("state", wifiState);
        }
    }
    if(state == "" || state == "down"){
        return;
    }
    if(state != "up"){
        qDebug() << "Unknown wifi state: " << state.c_str();
        return;
    }
    auto ip = exec("ip r | grep default | awk '{print $3}'");
    auto connected = !system(("echo -n > /dev/tcp/" + ip.substr(0, ip.length() - 1) + "/53").c_str());
    if(wifiConnected != connected){
        wifiConnected = connected;
        if(ui){
            ui->setProperty("connected", connected);
        }
    }
    auto link = std::stoi(exec("cat /proc/net/wireless | grep wlan0 | awk '{print $3}'"));
    if(wifiLink != link){
        wifiLink = link;
        if(ui){
            ui->setProperty("link", link);
        }
    }
    if(showWifiDb()){
        auto level = std::stoi(exec("cat /proc/net/wireless | grep wlan0 | awk '{print $4}'"));
        if(wifiLevel != level){
            wifiLevel = level;
            if(ui){
                ui->setProperty("level", level);
            }
        }
    }
}
void Controller::resetInactiveTimer(){
    if(filter->timer->isActive()){
        filter->timer->stop();
        filter->timer->start();
    }
}
void Controller::setAutomaticSleep(bool state){
    m_automaticSleep = state;
    if(root != nullptr){
        if(state){
            qDebug() << "Enabling automatic sleep";
            filter->timer->start();
        }else{
            qDebug() << "Disabling automatic sleep";
            filter->timer->stop();
        }
        emit automaticSleepChanged(state);
    }
}
void Controller::setColumns(int columns){
    m_columns = columns;
    if(root != nullptr){
        qDebug() << "Columns: " << columns;
        emit columnsChanged(columns);
    }
}
void Controller::setFontSize(int fontSize){
    m_fontSize= fontSize;
    if(root != nullptr){
        qDebug() << "Font Size: " << fontSize;
        emit fontSizeChanged(fontSize);
    }
}
void Controller::setShowWifiDb(bool state){
    m_showWifiDb = state;
    if(root != nullptr){
        qDebug() << "Show Wifi DB: " << state;
        emit showWifiDbChanged(state);
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
    m_sleepAfter= sleepAfter;
    if(root != nullptr){
        qDebug() << "Sleep After: " << sleepAfter << " minutes";
        filter->timer->setInterval(sleepAfter * 60 * 1000);
        emit sleepAfterChanged(sleepAfter);
    }
}
