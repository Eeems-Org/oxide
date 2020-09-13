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

#include "controller.h"

QSet<QString> settings = { "columns", "fontSize", "sleepAfter" };
QSet<QString> booleanSettings {"automaticSleep", "showWifiDb", "showBatteryPercent", "showBatteryTemperature" };
QList<QString> configDirectoryPaths = { "/opt/etc/draft", "/etc/draft", "/home/root /.config/draft" };
QList<QString> configFileDirectoryPaths = { "/opt/etc", "/etc", "/home/root /.config" };

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
                    qWarning() << "  Wrong format on " << line;
                    continue;
                }
                QString lhs = parts.at(0).trimmed();
                QString rhs = parts.at(1).trimmed();
                if (booleanSettings.contains(lhs)){
                    auto property = lhs.toStdString();
                    auto value = rhs.toLower();
                    auto boolValue = value == "true" || value == "t" || value == "y" || value == "yes" || value == "1";
                    setProperty(property.c_str(), boolValue);
                    qDebug() << "  " << property.c_str() << ": " << boolValue;
                }else if(settings.contains(lhs)){
                    auto property = lhs.toStdString();
                    setProperty(property.c_str(), rhs);
                    qDebug() << "  " << property.c_str() << ": " << rhs.toStdString().c_str();
                }
            }
        }
        configFile->close();
    }
    qDebug() << "Finished parsing config file.";
    if(root){
        qDebug() << "Updating UI with settings from config file...";
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
        qDebug() << "Finished updating UI.";
    }
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
            qDebug() <<  " " << propertyName.c_str() << ": " << value.c_str();
            buffer << propertyName << "=" << value << std::endl;
        }
    }else if (!configFile->open(QIODevice::WriteOnly | QIODevice::Text)) {
        qCritical() << "Unable to create the config file. " << configFile->fileName();
        return;
    }
    for(QString item : items){
        auto propertyName = item.toStdString();
        auto value = property(item.toStdString().c_str()).toString().toStdString();
        qDebug() <<  " " << propertyName.c_str() << ": " << value.c_str();
        buffer << propertyName << "=" << value << std::endl;
    }
    configFile->resize(0);
    stream << QString::fromStdString(buffer.str());
    configFile->close();
    qDebug() << "Dont saving configuration.";
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
                    if(line.startsWith("#")){
                        continue;
                    }
                    QStringList parts = line.split("=");
                    if(parts.length() != 2){
                        qWarning() << "wrong format on " << line;
                        continue;
                    }
                    QString lhs = parts.at(0);
                    QString rhs = parts.at(1);
                    QSet<QString> known = { "name", "desc", "call", "term" };
                    if(rhs != ":" && rhs != ""){
                        if (known.contains(lhs)){
                            app->setProperty(lhs.toUtf8(), rhs);
                        }else if (lhs == "imgFile"){
                            app->setProperty(lhs.toUtf8(), "file:" + configDirectoryPath + "/icons/" + rhs + ".png");
                        }
                    }
                }
                if(app ->ok()){
                    app->inputManager = &inputManager;
                    result.append(app );
                }
                file.close();
            }
        }
    }
    // Sort by name
    std::sort(result.begin(), result.end(), [=](const QObject* a, const QObject* b) -> bool {
        return a->property("name") < b->property("name");
    });
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
void Controller::updateUIElements(){
    if(wifiApi == nullptr){
        return;
    }
    QObject* ui = root->findChild<QObject*>("wifiState");
    if(!ui){
        qDebug() << "Can't find wifiState";
    }
    int link = wifiApi->link();
    if(wifiLink != link){
        wifiLink = link;
        updateUI(ui, "link", link);
        qDebug() << ("Wifi link: " + to_string(link)).c_str();
    }
    updateHiddenUIElements();
}
void Controller::updateHiddenUIElements(){
    if(showWifiDb()){
        QObject* ui = root->findChild<QObject*>("wifiState");
        if(ui){
            int level = 0;
            if(wifiConnected){
                level = std::stoi(exec("cat /proc/net/wireless | grep wlan0 | awk '{print $4}'"));
            }
            if(wifiLevel != level){
                wifiLevel = level;
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
    if(state){
        updateHiddenUIElements();
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
    if(state){
        updateHiddenUIElements();
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