#if defined __cplusplus
#include <algorithm>
#include <cstdlib>
#include <epframebuffer.h>
#include <ext/stdio_filebuf.h>
#include <fcntl.h>
#include <fstream>
#include <grp.h>
#include <iostream>
#include <linux/fb.h>
#include <linux/input.h>
#include <pwd.h>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QException>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>
#include <QMutableListIterator>
#include <QMutex>
#include <QObject>
#include <QPainter>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QSettings>
#include <QtCore/QByteArray>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QVariant>
#include <QtDBus/QtDBus>
#include <QtDBus>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QUuid>
#include <signal.h>
#include <sstream>
#include <stdexcept>
#include <stdio.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <systemd/sd-journal.h>
#include <unistd.h>
#include <unordered_map>
#include <zlib.h>

#include "apibase.h"
#include "mxcfb.h"
#include "supplicant.h"
#endif