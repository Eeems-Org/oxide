#ifndef HANDLER_H
#define HANDLER_H
#pragma once

#include <QQmlApplicationEngine>
#include <QObject>
#include <QQuickItem>
#include <QDebug>
#include <QtGui>
#include <time.h>
#include "appsview.h"

extern time_t lastReturn;

class Handler : public QObject{
    Q_OBJECT
public:
    Handler(AppsView* appsView, std::string lT, std::string term, QGuiApplication* app, QQmlApplicationEngine* engine, QObject* obj);
    ~Handler();
    bool eventFilter(QObject* obj, QEvent* event);
    void handleEvent();
private:
    AppsView* appsView;
    std::string link;
    std::string term;
    QObject* ef;
    QGuiApplication* app;
    QQmlApplicationEngine* engine;
};

#endif // HANDLER_H
