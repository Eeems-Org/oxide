#include "controller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QProcess>
#include <QStringList>

Controller::Controller(QObject* parent)
  : QObject(parent)
  , m_currentPath("/home/root") {
  loadDirectory();
}

QString
Controller::currentPath() const {
  return m_currentPath;
}

QVariantList
Controller::entries() const {
  return m_entries;
}

QString
Controller::status() const {
  return m_status;
}

bool
Controller::busy() const {
  return m_busy;
}

QString
Controller::homePath() const {
  return "/home/root";
}

void
Controller::setBusy(bool busy) {
  if (m_busy == busy) {
    return;
  }
  m_busy = busy;
  emit busyChanged();
}

void
Controller::loadDirectory() {
  m_entries.clear();
  QDir dir(m_currentPath);
  if (!dir.exists()) {
    m_status = "Directory does not exist: " + m_currentPath;
    emit statusChanged();
    emit entriesChanged();
    return;
  }
  QFileInfoList list = dir.entryInfoList(
    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
    QDir::DirsFirst | QDir::Name | QDir::IgnoreCase
  );
  for (const QFileInfo& info : list) {
    QVariantMap item;
    item["name"] = info.fileName();
    item["path"] = info.absoluteFilePath();
    item["isDir"] = info.isDir();
    item["size"] = info.size();
    item["modified"] = info.lastModified().toString(Qt::ISODate);
    item["hidden"] = info.isHidden();
    m_entries.append(item);
  }
  m_status.clear();
  emit statusChanged();
  emit entriesChanged();
}

void
Controller::openPath(const QString& path) {
  if (path.isEmpty()) {
    openPath(homePath());
    return;
  }
  QFileInfo info(path);
  if (!info.exists()) {
    m_status = "Path does not exist: " + path;
    emit statusChanged();
    return;
  }
  if (!info.isDir()) {
    m_status = "Not a directory: " + path;
    emit statusChanged();
    return;
  }
  m_currentPath = info.absoluteFilePath();
  emit currentPathChanged();
  loadDirectory();
}

void
Controller::goUp() {
  QDir dir(m_currentPath);
  if (dir.cdUp()) {
    openPath(dir.absolutePath());
  }
}

void
Controller::goHome() {
  openPath(homePath());
}

void
Controller::refresh() {
  loadDirectory();
}

void
Controller::createFolder(const QString& name) {
  if (name.isEmpty() || name.contains('/') || name == "." || name == "..") {
    m_status = "Invalid folder name";
    emit statusChanged();
    return;
  }
  setBusy(true);
  QDir dir(m_currentPath);
  if (dir.mkdir(name)) {
    m_status = "Created folder: " + name;
  } else {
    m_status = "Could not create folder: " + name;
  }
  emit statusChanged();
  loadDirectory();
  setBusy(false);
}

void
Controller::renamePath(const QString& oldPath, const QString& newName) {
  if (
    newName.isEmpty() || newName.contains('/') || newName == "." ||
    newName == ".."
  ) {
    m_status = "Invalid name";
    emit statusChanged();
    return;
  }
  QFileInfo info(oldPath);
  if (!info.exists()) {
    m_status = "Path does not exist: " + oldPath;
    emit statusChanged();
    return;
  }
  QString newPath = info.absolutePath() + "/" + newName;
  if (QFile::exists(newPath)) {
    m_status = "Destination already exists: " + newPath;
    emit statusChanged();
    return;
  }
  setBusy(true);
  QDir dir;
  if (dir.rename(oldPath, newPath)) {
    m_status = "Renamed to: " + newName;
  } else {
    m_status = "Could not rename: " + oldPath;
  }
  emit statusChanged();
  loadDirectory();
  setBusy(false);
}

void
Controller::removePaths(const QStringList& paths) {
  setBusy(true);
  QStringList errors;
  for (const QString& path : paths) {
    QFileInfo info(path);
    if (info.isDir()) {
      QDir dir(path);
      if (!dir.removeRecursively()) {
        errors.append(path);
      }
    } else {
      if (!QFile::remove(path)) {
        errors.append(path);
      }
    }
  }
  if (errors.isEmpty()) {
    m_status = "Deleted selected items";
  } else {
    m_status = "Could not delete: " + errors.join(", ");
  }
  emit statusChanged();
  loadDirectory();
  setBusy(false);
}

bool
Controller::copyRecursive(const QString& source, const QString& destination) {
  QFileInfo srcInfo(source);
  if (srcInfo.isDir()) {
    QDir destDir(destination);
    if (!destDir.mkpath(".")) {
      return false;
    }
    QDir srcDir(source);
    QFileInfoList entries = srcDir.entryInfoList(
      QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden
    );
    for (const QFileInfo& entry : entries) {
      QString srcPath = entry.absoluteFilePath();
      QString destPath = destination + "/" + entry.fileName();
      if (!copyRecursive(srcPath, destPath)) {
        return false;
      }
    }
    return true;
  }
  if (QFile::exists(destination)) {
    QFile::remove(destination);
  }
  return QFile::copy(source, destination);
}

void
Controller::copyPaths(const QStringList& paths, const QString& destination) {
  QFileInfo destInfo(destination);
  if (!destInfo.exists() || !destInfo.isDir()) {
    m_status = "Destination is not a directory: " + destination;
    emit statusChanged();
    return;
  }
  setBusy(true);
  QStringList errors;
  for (const QString& path : paths) {
    QFileInfo info(path);
    QString target = destination + "/" + info.fileName();
    if (!copyRecursive(path, target)) {
      errors.append(path);
    }
  }
  if (errors.isEmpty()) {
    m_status = "Copied selected items";
  } else {
    m_status = "Could not copy: " + errors.join(", ");
  }
  emit statusChanged();
  if (m_currentPath == destination) {
    loadDirectory();
  }
  setBusy(false);
}

void
Controller::movePaths(const QStringList& paths, const QString& destination) {
  QFileInfo destInfo(destination);
  if (!destInfo.exists() || !destInfo.isDir()) {
    m_status = "Destination is not a directory: " + destination;
    emit statusChanged();
    return;
  }
  setBusy(true);
  QStringList errors;
  for (const QString& path : paths) {
    QFileInfo info(path);
    QString target = destination + "/" + info.fileName();
    if (QFile::exists(target)) {
      errors.append(path);
      continue;
    }
    QDir dir;
    if (!dir.rename(path, target)) {
      if (copyRecursive(path, target)) {
        QFileInfo srcInfo(path);
        if (srcInfo.isDir()) {
          QDir(path).removeRecursively();
        } else {
          QFile::remove(path);
        }
      } else {
        errors.append(path);
      }
    }
  }
  if (errors.isEmpty()) {
    m_status = "Moved selected items";
  } else {
    m_status = "Could not move: " + errors.join(", ");
  }
  emit statusChanged();
  loadDirectory();
  setBusy(false);
}

void
Controller::openFile(const QString& path) {
  QFileInfo info(path);
  if (info.isDir()) {
    openPath(path);
    return;
  }
  QProcess::startDetached("/home/root/.vellum/bin/xdg-open", QStringList{path});
}