#include "handler.h"
#include <QtQuick>
#include <QString>
#include <iostream>
#include <fstream>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

time_t lastReturn;

Handler::Handler(
    AppsView* appsView, std::string lT, std::string term, QGuiApplication* app,
    QQmlApplicationEngine* engine, QObject* obj)
: appsView(appsView),
  link(lT),
  term(term),
  ef(obj),
  app(app),
  engine(engine)
{
    time(&lastReturn);
}

bool Handler::eventFilter(QObject* obj, QEvent* event){
    if (event->type() != QEvent::MouseButtonRelease){
        return false;
    }
    time_t tim;
    time(&tim);
    // 1 second cooldown before opening again!
    if(difftime(tim, lastReturn) > 1){
        handleEvent();
    }
    return true;
}

void Handler::handleEvent(){
    std::cout << "Setting termfile /etc/draft/.terminate" << std::endl;
    std::ofstream termfile;
    termfile.open("/etc/draft/.terminate");
    termfile << term << std::endl;

    std::cout << "Running command " << link << "..." << std::endl;
    system((link).c_str());

    // Don't exit any more - just head back to the launcher!
    std::cout << "Running again" << std::endl;

    engine->rootObjects().first()->setProperty("reloaded", QVariant(true));
    app->processEvents();
    std::cout << "Screen repaint forced" << std::endl;

    // Cooldown
    time(&lastReturn);

}

Handler::~Handler(){
    ef->removeEventFilter(this);
}
