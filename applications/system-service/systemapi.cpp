#include "systemapi.h"
#include "appsapi.h"

void SystemAPI::PrepareForSleep(bool suspending){
    if(suspending){
        auto path = appsAPI->currentApplication();
        if(path.path() != "/"){
            resumeApp = appsAPI->getApplication(path);
            resumeApp->pause(false);
        }else{
            resumeApp = nullptr;
        }
        drawSleepImage();
        qDebug() << "Suspending...";
        buttonHandler->setEnabled(false);
        releaseSleepInhibitors();
    }else{
        inhibitSleep();
        qDebug() << "Resuming...";
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
        if(resumeApp == nullptr){
            resumeApp = appsAPI->getApplication(appsAPI->startupApplication());
        }
        resumeApp->resume();
        buttonHandler->setEnabled(true);
    }
}
