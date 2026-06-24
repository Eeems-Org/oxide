#include "common.h"

#include <QDirIterator>

QTextStream&
qStdOut() {
  static QTextStream ts(stdout);
  return ts;
}
QTextStream&
qStdErr() {
  static QTextStream ts(stderr);
  return ts;
}

bool
query(const QString& launcher, const QStringList& args, QString* output) {
  auto path = QString(SHARE_DIR) + "/" + launcher;
  if (!QFile::exists(path)) {
    qStdErr() << "Error: Launcher " << launcher << " does not exist\n";
    return false;
  }
  QProcess proc;
  proc.setProgram(path);
  proc.setArguments(args);
  proc.setProcessChannelMode(QProcess::ForwardedErrorChannel);
  if (output != nullptr) {
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start();
    proc.waitForFinished(-1);
    *output += QString::fromLocal8Bit(proc.readAllStandardOutput());
  } else {
    proc.setInputChannelMode(QProcess::ForwardedInputChannel);
    proc.setProcessChannelMode(QProcess::ForwardedChannels);
    proc.start();
    proc.waitForFinished(-1);
  }
  return proc.exitCode() == 0;
}

bool
queryCurrent(const QStringList& args, QString* output) {
  return query("current", args, output);
}

bool
queryActive(const QStringList& args, QString* output) {
  for (const auto& launcher : launchersActive()) {
    if (!query(launcher, args, output)) {
      return false;
    }
  }
  return true;
}

QStringList
launchers() {
  QStringList result;
  QDirIterator it(
    SHARE_DIR, QDir::Files | QDir::Executable, QDirIterator::NoIteratorFlags
  );
  while (it.hasNext()) {
    it.next();
    auto name = it.fileName();
    if (name != "current") {
      result.append(name);
    }
  }
  return result;
}

QStringList
launchersActive() {
  QStringList result;
  for (const auto& launcher : launchers()) {
    if (query(launcher, {"is-active"})) {
      result.append(launcher);
    }
  }
  return result;
}

QStringList
launchersEnabled() {
  auto current = currentLauncher();
  QStringList result;
  for (const auto& launcher : launchers()) {
    if (query(launcher, {"is-enabled"}) || launcher == current) {
      result.append(launcher);
    }
  }
  return result;
}

QString
currentLauncher() {
  auto symlink = QString(SHARE_DIR) + "/current";
  if (!QFile::exists(symlink)) {
    QFile::link("none", symlink);
  }
  auto target = QFile::symLinkTarget(symlink);
  if (target.isEmpty()) {
    return {};
  }
  return QFileInfo(target).baseName();
}

void
checkActiveLaunchers() {
  auto active = launchersActive();
  if (active.isEmpty()) {
    qStdErr() << "Error: No launcher is currently active!\n";
    exit(EXIT_FAILURE);
  }
  if (active.size() > 1) {
    qStdErr() << "Error: More than one launcher is currently active!\n"
              << "Active launchers: " << active.join(" ") << "\n";
    exit(EXIT_FAILURE);
  }
}

void
checkLauncherHasApp(const QString& appName) {
  QString appsOutput;
  queryActive({"apps"}, &appsOutput);
  auto apps = appsOutput.split('\n', Qt::SkipEmptyParts);
  if (!apps.contains(appName)) {
    qStdErr() << "Error: Unknown application: " << appName << "\n";
    exit(EXIT_FAILURE);
  }
}
