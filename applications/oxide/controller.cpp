#include "controller.h"
#include <QIODevice>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QSet>
#include <QDebug>

QList<QObject*> Controller::getApps(){

    QString configDir = "/opt/etc/draft";
    QList<QObject*> result;
    // If the config directory doesn't exist,
    // then print an error and stop.
    QDir directory(configDir);
    if (!directory.exists() || configDir.isEmpty())
    {
        qCritical() << "Failed to read directory - it does not exist.> " << configDir;
        return result;
    }


    directory.setFilter( QDir::Files | QDir::NoSymLinks | QDir::NoDot | QDir::NoDotDot);

    auto images = directory.entryInfoList(QDir::NoFilter,QDir::SortFlag::Name);
    foreach(QFileInfo fi, images) {
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
                    app->setProperty(lhs.toUtf8(), "file:" + configDir + "/icons/" + rhs + ".png");
                }else{
                    app->setProperty(lhs.toUtf8(), "qrc:/icon.png");
                }
            }
        }
        if(app ->ok()){
            result.append(app );
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
    return QString::fromStdString(std::to_string(battery_level) + "%");
}

void Controller::resetInactiveTimer(){
    filter->timer->stop();
    filter->timer->start();
}
