#pragma once

#include <QObject>

#include "appsapi.h"
#include "screenapi.h"

class Controller : public QObject{
    Q_OBJECT
public:
    explicit Controller(QObject* parent = nullptr) : QObject(parent){}
    Q_INVOKABLE void screenshot(){ screenAPI->screenshot(); }
    Q_INVOKABLE void taskSwitcher(){ appsAPI->openTaskSwitcher(); }
    Q_INVOKABLE void processManager(){ appsAPI->openTaskManager(); }
    Q_INVOKABLE void back(){ appsAPI->previousApplication(); }
    Q_INVOKABLE void lock(){ appsAPI->openLockScreen(); }
    Q_INVOKABLE void terminal(){ appsAPI->openTerminal(); }
};
