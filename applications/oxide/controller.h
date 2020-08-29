#pragma once

#include <QObject>
#include "appitem.h"
#include "eventfilter.h"

class Controller : public QObject
{
    Q_OBJECT
public:
    EventFilter* filter;
    QObject* root;
    explicit Controller(QObject* parent = 0) : QObject(parent){}
    Q_INVOKABLE void loadSettings();
    Q_INVOKABLE void saveSettings();
    Q_INVOKABLE QList<QObject*> getApps();
    Q_INVOKABLE void powerOff();
    Q_INVOKABLE void suspend();
    Q_INVOKABLE void killXochitl();
    Q_INVOKABLE QString getBatteryLevel();
    Q_INVOKABLE void resetInactiveTimer();
    Q_PROPERTY(bool automaticSleep MEMBER m_automaticSleep WRITE setAutomaticSleep NOTIFY automaticSleepChanged);
    bool automaticSleep() const {
        return m_automaticSleep;
    };
    void setAutomaticSleep(bool);
    Q_PROPERTY(int columns MEMBER m_columns WRITE setColumns NOTIFY columnsChanged);
    int columns() const {
        return m_columns;
    };
    void setColumns(int);
    Q_PROPERTY(int fontSize MEMBER m_fontSize WRITE setFontSize NOTIFY fontSizeChanged);
    int fontSize() const {
        return m_fontSize;
    };
    void setFontSize(int);
signals:
    void automaticSleepChanged(bool);
    void columnsChanged(int);
    void fontSizeChanged(int);
private:
    bool m_automaticSleep = true;
    int m_columns = 6;
    int m_fontSize = 23;
};
