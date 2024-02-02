#pragma once

#include <QObject>

#include "appsapi.h"
#include "screenapi.h"
#include "systemapi.h"

class Controller : public QObject{
    Q_OBJECT
    Q_PROPERTY(bool leftSwipeEnabled READ leftSwipeEnabled NOTIFY leftSwipeEnabledChanged)
    Q_PROPERTY(bool rightSwipeEnabled READ rightSwipeEnabled NOTIFY rightSwipeEnabledChanged)
    Q_PROPERTY(bool upSwipeEnabled READ upSwipeEnabled NOTIFY upSwipeEnabledChanged)
    Q_PROPERTY(bool downSwipeEnabled READ downSwipeEnabled NOTIFY downSwipeEnabledChanged)
    Q_PROPERTY(int leftSwipeLength READ leftSwipeLength NOTIFY leftSwipeLengthChanged)
    Q_PROPERTY(int rightSwipeLength READ rightSwipeLength NOTIFY rightSwipeLengthChanged)
    Q_PROPERTY(int upSwipeLength READ upSwipeLength NOTIFY upSwipeLengthChanged)
    Q_PROPERTY(int downSwipeLength READ downSwipeLength NOTIFY downSwipeLengthChanged)

public:
    explicit Controller(QObject* parent = nullptr) : QObject(parent){
        connect(systemAPI, &SystemAPI::swipeLengthChanged, this, [this](int swipe, int length){
            switch((SystemAPI::SwipeDirection)swipe){
                case SystemAPI::Left:
                    emit leftSwipeLengthChanged(length);
                    break;
                case SystemAPI::Right:
                    emit rightSwipeLengthChanged(length);
                    break;
                case SystemAPI::Up:
                    emit upSwipeLengthChanged(length);
                    break;
                case SystemAPI::Down:
                    emit downSwipeLengthChanged(length);
                    break;
                default:
                    break;
            }
        });
        connect(systemAPI, &SystemAPI::swipeEnabledChanged, this, [this](int swipe, bool enabled){
            switch((SystemAPI::SwipeDirection)swipe){
                case SystemAPI::Left:
                    emit leftSwipeEnabledChanged(enabled);
                    break;
                case SystemAPI::Right:
                    emit rightSwipeEnabledChanged(enabled);
                    break;
                case SystemAPI::Up:
                    emit upSwipeEnabledChanged(enabled);
                    break;
                case SystemAPI::Down:
                    emit downSwipeEnabledChanged(enabled);
                    break;
                default:
                    break;
            }
        });
    }
    Q_INVOKABLE void screenshot(){ screenAPI->screenshot(); }
    Q_INVOKABLE void taskSwitcher(){ appsAPI->openTaskSwitcher(); }
    Q_INVOKABLE void processManager(){ appsAPI->openTaskManager(); }
    Q_INVOKABLE void back(){ appsAPI->previousApplication(); }
    Q_INVOKABLE void lock(){ appsAPI->openLockScreen(); }
    Q_INVOKABLE void terminal(){ appsAPI->openTerminal(); }
    Q_INVOKABLE void close(){
        auto path = appsAPI->currentApplicationNoSecurityCheck();
        if(path.path() == "/"){
            return;
        }
        if(path == appsAPI->lockscreenApplication()){
            return;
        }
        if(path == appsAPI->startupApplication()){
            return;
        }
        if(path == appsAPI->taskSwitcherApplication()){
            return;
        }
        auto currentApplication = appsAPI->getApplication(path);
        if(
            currentApplication == nullptr
            || currentApplication->stateNoSecurityCheck() == Application::Inactive
        ){
            return;
        }
        currentApplication->stop();
    }
    Q_INVOKABLE void toggleSwipes(){ systemAPI->toggleSwipes(); }

    bool leftSwipeEnabled(){ return systemAPI->getSwipeEnabled(SystemAPI::Left); }
    bool rightSwipeEnabled(){ return systemAPI->getSwipeEnabled(SystemAPI::Right); }
    bool upSwipeEnabled(){ return systemAPI->getSwipeEnabled(SystemAPI::Up); }
    bool downSwipeEnabled(){ return systemAPI->getSwipeEnabled(SystemAPI::Down); }
    int leftSwipeLength(){ return systemAPI->getSwipeLength(SystemAPI::Left); }
    int rightSwipeLength(){ return systemAPI->getSwipeLength(SystemAPI::Right); }
    int upSwipeLength(){ return systemAPI->getSwipeLength(SystemAPI::Up); }
    int downSwipeLength(){ return systemAPI->getSwipeLength(SystemAPI::Down); }

signals:
    void leftSwipeEnabledChanged(bool);
    void rightSwipeEnabledChanged(bool);
    void upSwipeEnabledChanged(bool);
    void downSwipeEnabledChanged(bool);
    void leftSwipeLengthChanged(int);
    void rightSwipeLengthChanged(int);
    void upSwipeLengthChanged(int);
    void downSwipeLengthChanged(int);
};
