#pragma once

#include "icommand.h"

#include <liboxide.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTextStream>

QTextStream&
qStdOut();
QTextStream&
qStdErr();

#define SHARE_DIR "/home/root/.vellum/share/launcherctl"

bool
query(
  const QString& launcher,
  const QStringList& args,
  QString* output = nullptr
);

bool
queryCurrent(const QStringList& args, QString* output = nullptr);

bool
queryActive(const QStringList& args, QString* output = nullptr);

QStringList
launchers();

QStringList
launchersActive();

QStringList
launchersEnabled();

QString
currentLauncher();

void
checkActiveLaunchers();

void
checkLauncherHasApp(const QString& appName);
