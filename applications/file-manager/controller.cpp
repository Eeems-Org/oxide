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
  for (const QFileInfo& info : dir.entryInfoList(
         QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden,
         QDir::DirsFirst | QDir::Name | QDir::IgnoreCase
       )) {
    m_entries.append(
      QVariantMap{
        {"name",     info.fileName()                          },
        {"path",     info.absoluteFilePath()                  },
        {"isDir",    info.isDir()                             },
        {"size",     info.size()                              },
        {"modified", info.lastModified().toString(Qt::ISODate)},
        {"hidden",   info.isHidden()                          }
    }
    );
  }
  emit statusChanged();
  emit entriesChanged();
}

void
Controller::clearStatus() {
  if (m_status.isEmpty()) {
    return;
  }
  m_status.clear();
  emit statusChanged();
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
  clearStatus();
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
  m_status = QString(dir.mkdir(name) ? "Created" : "Could not create") +
             " folder: " + name;
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
  m_status =
    QString(dir.rename(oldPath, newPath) ? "Renamed to" : "Could not rename") +
    ": " + newName;
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
    if (!((info.isDir() && !info.isSymLink()) ? QDir(path).removeRecursively()
                                              : QFile::remove(path))) {
      errors.append(path);
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
  QFileInfo destInfo(destination);
  if (srcInfo.exists() && destInfo.exists()) {
    QString src = srcInfo.canonicalFilePath();
    QString dst = destInfo.canonicalFilePath();
    if (!src.isEmpty() && !dst.isEmpty() && src == dst) {
      return true;
    }
  }

  if (srcInfo.isSymLink()) {
    QString target = srcInfo.symLinkTarget();
    if (QFile::exists(destination)) {
      QFile::remove(destination);
    }
    return QFile::link(target, destination);
  }
  if (srcInfo.isDir()) {
    QDir destDir(destination);
    if (!destDir.mkpath(".")) {
      return false;
    }
    QDir srcDir(source);
    QFileInfoList entries = srcDir.entryInfoList(
      QDir::NoFilter | QDir::NoDotAndDotDot,
      QDir::DirsFirst | QDir::Name | QDir::IgnoreCase
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
  QString tempDest = destination + ".tmp";
  if (QFile::exists(tempDest)) {
    QFile::remove(tempDest);
  }
  if (!QFile::copy(source, tempDest)) {
    return false;
  }
  if (QFile::exists(destination)) {
    QFile::remove(destination);
  }
  return QFile::rename(tempDest, destination);
}

void
Controller::copyPaths(const QStringList& paths, const QString& destination) {
  QFileInfo destInfo(destination);
  if (!destInfo.exists() || !destInfo.isDir()) {
    m_status = "Destination is not a directory: " + destination;
    emit statusChanged();
    return;
  }
  QString destCanon = destInfo.canonicalFilePath();
  for (const QString& path : paths) {
    QFileInfo srcInfo(path);
    QString srcCanon = srcInfo.canonicalFilePath();
    if (!srcCanon.isEmpty() && !destCanon.isEmpty()) {
      if (destCanon == srcCanon || destCanon.startsWith(srcCanon + "/")) {
        m_status = "Cannot copy: destination is inside source";
        emit statusChanged();
        return;
      }
    }
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
  QString dst = destInfo.canonicalFilePath();
  for (const QString& path : paths) {
    QFileInfo srcInfo(path);
    QString src = srcInfo.canonicalFilePath();
    if (!src.isEmpty() && !dst.isEmpty()) {
      if (dst == src || dst.startsWith(src + "/")) {
        m_status = "Cannot move: destination is inside source";
        emit statusChanged();
        return;
      }
    }
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
        if (!(srcInfo.isDir() && !srcInfo.isSymLink()
                ? QDir(path).removeRecursively()
                : QFile::remove(path))) {
          errors.append(path + " (copy succeeded but source remove failed)");
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
  QProcess::startDetached("/home/root/.vellum/bin/xdg-open", {path});
}
