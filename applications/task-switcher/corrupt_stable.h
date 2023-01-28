#if defined __cplusplus
#include <cstdlib>
#include <epframebuffer.h>
#include <fstream>
#include <iostream>
#include <liboxide.h>
#include <liboxide/eventfilter.h>
#include <QDebug>
#include <QGuiApplication>
#include <QImage>
#include <QObject>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QQuickItem>
#include <QScreen>
#include <QtPlugin>
#include <QtQuick>
#include <signal.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "application_interface.h"
#include "appsapi_interface.h"
#include "dbusservice_interface.h"
#include "screenapi_interface.h"

#include "controller.h"
#include "screenprovider.h"
#endif
