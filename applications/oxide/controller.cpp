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

QString configDirectoryPath = "/opt/etc/draft";
QDir configDirectory(configDirectoryPath);
QString configFilePath = configDirectoryPath + "/conf";
QFile configFile(configFilePath);
QSet<QString> settings = { "automaticSleep", "columns" };
bool reading = false;

bool validateConfigFolder(){
    if (!configDirectory.exists() || configDirectoryPath.isEmpty())
    {
        qCritical() << "Failed to read directory - it does not exist. " << configDirectoryPath;
        return false;
    }
    return true;
}
void Controller::loadSettings(){
    // If the config directory doesn't exist,
    // then print an error and stop.
    qDebug() << "parsing config file ";
    if(!validateConfigFolder()){
        return;
    }
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qCritical() << "Couldn't read the config file. " << configFile.fileName();
        return;
    }
    QTextStream in(&configFile);
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
            if (lhs == "automaticSleep"){
                auto value = rhs.toLower();
                setAutomaticSleep(value == "true" || value == "t" || value == "y" || value == "yes" || value == "1");
            }else if(settings.contains(lhs)){
                setProperty(lhs.toStdString().c_str(), rhs);
            }
        }
    }
    configFile.close();
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
}
void Controller::saveSettings(){
    if(!validateConfigFolder()){
        return;
    }
    if (!configFile.open(QIODevice::ReadWrite | QIODevice::Text)) {
        qCritical() << "Unable to read the config file. " << configFile.fileName();
        return;
    }
    QTextStream stream(&configFile);
    QSet<QString> items = settings;
    items.detach();
    std::stringstream buffer;
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
        if (lhs == "automaticSleep"){
            if(automaticSleep()){
                rhs = "yes";
            }else{
                rhs = "no";
            }
            items.remove("automaticSleep");
        }else if(items.contains(lhs)){
            rhs = property(lhs.toStdString().c_str()).toString();
            items.remove(lhs);
        }
        buffer << lhs.toStdString() << "=" << rhs.toStdString() << std::endl;
    }
    for(QString item : items){
        buffer << item.toStdString() << property(item.toStdString().c_str()).toString().toStdString() << std::endl;
    }
    configFile.resize(0);
    stream << QString::fromStdString(buffer.str());
    configFile.close();
}
QList<QObject*> Controller::getApps(){
    QList<QObject*> result;
    // If the config directory doesn't exist,
    // then print an error and stop.
    if(!validateConfigFolder()){
        return result;
    }
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
                        app->setProperty(lhs.toUtf8(), "qrc:/icon.png");
                    }
                }
            }
            if(app ->ok()){
                result.append(app );
            }
            file.close();
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
int readInt(std::string path) {

    QFile file(path.c_str());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "Couldn't find the file " << path.c_str();
        return 0;
    }
    QTextStream in(&file);
    std::string text = in.readLine().toStdString();
    int number = std::stoi(text);

    return number;
}
QString Controller::getBatteryLevel() {
    int charge_now = readInt(
        "/sys/class/power_supply/bq27441-0/charge_now"
    );
    int charge_full = readInt(
        "/sys/class/power_supply/bq27441-0/charge_full_design"
    );

    int battery_level = 100;
    if (charge_now < charge_full) {
        battery_level = (charge_now * 100/ charge_full);
    }
    qDebug() << "Got battery level " << battery_level;
    return QString::fromStdString(std::to_string(battery_level));
}
void Controller::resetInactiveTimer(){
    filter->timer->stop();
    filter->timer->start();
}
void Controller::setAutomaticSleep(bool state){
    m_automaticSleep = state;
    if(state){
        qDebug() << "Enabling automatic sleep";
        filter->timer->start();
    }else{
        qDebug() << "Disabling automatic sleep";
        filter->timer->stop();
    }
    emit automaticSleepChanged(state);
}

void Controller::setColumns(int columns){
    m_columns = columns;
    qDebug() << "Columns: " << columns;
    emit columnsChanged(columns);
}
void Controller::setFontSize(int fontSize){
    m_fontSize= fontSize;
    qDebug() << "Font Size: " << fontSize;
    emit fontSizeChanged(fontSize);
}
