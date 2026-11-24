#include "controller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QProcess>
#include <QTemporaryFile>
#include <QThread>
#include <liboxide/threading.h>

Controller::Controller(QObject* parent)
  : QObject(parent)
  , m_currentPath("/home/root")
  , m_workerThread(new QThread(this)) {
  m_workerThread->start();
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
    openPath("/home/root");
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
  openPath("/home/root");
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
  if (m_busy) {
    return;
  }
  m_busy = true;
  Oxide::runLater(m_workerThread, [this, name]() {
    QString status =
      QString(
        QDir(m_currentPath).mkdir(name) ? "Created" : "Could not create"
      ) +
      " folder: " + name;
    Oxide::dispatchToMainThread([this, status]() {
      m_status = status;
      emit statusChanged();
      loadDirectory();
      m_busy = false;
    });
  });
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
  if (m_busy) {
    return;
  }
  m_busy = true;
  Oxide::runLater(m_workerThread, [this, oldPath, newPath, newName]() {
    QString status =
      QString(
        QFile::rename(oldPath, newPath) ? "Renamed to" : "Could not rename"
      ) +
      ": " + newName;
    Oxide::dispatchToMainThread([this, status]() {
      m_status = status;
      emit statusChanged();
      loadDirectory();
      m_busy = false;
    });
  });
}

void
Controller::removePaths(const QStringList& paths) {
  if (m_busy) {
    return;
  }
  m_busy = true;
  Oxide::runLater(m_workerThread, [this, paths]() {
    QStringList errors;
    for (const QString& path : paths) {
      QFileInfo info(path);
      if (!((info.isDir() && !info.isSymLink()) ? QDir(path).removeRecursively()
                                                : QFile::remove(path))) {
        errors.append(path);
      }
    }
    QString status;
    if (errors.isEmpty()) {
      status = "Deleted selected items";
    } else {
      status = "Could not delete: " + errors.join(", ");
    }
    Oxide::dispatchToMainThread([this, status]() {
      m_status = status;
      emit statusChanged();
      loadDirectory();
      m_busy = false;
    });
  });
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
      QDir::Files | QDir::Dirs | QDir::Hidden | QDir::System |
        QDir::NoDotAndDotDot,
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
  QDir destDir = destInfo.dir();
  QTemporaryFile tempFile;
  tempFile.setAutoRemove(false);
  tempFile.setFileTemplate(destDir.filePath("XXXXXX"));
  if (!tempFile.open()) {
    return false;
  }

  QFile srcFile(source);
  if (
    !srcFile.open(QIODevice::ReadOnly) || !srcFile.copy(tempFile.fileName()) ||
    !tempFile.flush()
  ) {
    tempFile.remove();
    return false;
  }

  QString tempDest = tempFile.fileName();
  tempFile.close();

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
  QString dst = destInfo.canonicalFilePath();
  for (const QString& path : paths) {
    QFileInfo srcInfo(path);
    QString src = srcInfo.canonicalFilePath();
    if (!src.isEmpty() && !dst.isEmpty()) {
      if (dst == src || dst.startsWith(src + "/")) {
        m_status = "Cannot copy: destination is inside source";
        emit statusChanged();
        return;
      }
    }
  }
  if (m_busy) {
    return;
  }
  m_busy = true;
  Oxide::runLater(m_workerThread, [this, paths, destination]() {
    QStringList errors;
    for (const QString& path : paths) {
      QFileInfo info(path);
      QString target = destination + "/" + info.fileName();
      if (!copyRecursive(path, target)) {
        errors.append(path);
      }
    }
    QString status;
    if (errors.isEmpty()) {
      status = "Copied selected items";
    } else {
      status = "Could not copy: " + errors.join(", ");
    }
    Oxide::dispatchToMainThread([this, status, destination]() {
      m_status = status;
      emit statusChanged();
      if (m_currentPath == destination) {
        loadDirectory();
      }
      m_busy = false;
    });
  });
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
  if (m_busy) {
    return;
  }
  m_busy = true;
  Oxide::runLater(m_workerThread, [this, paths, destination]() {
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
    QString status;
    if (errors.isEmpty()) {
      status = "Moved selected items";
    } else {
      status = "Could not move: " + errors.join(", ");
    }
    Oxide::dispatchToMainThread([this, status]() {
      m_status = status;
      emit statusChanged();
      loadDirectory();
      m_busy = false;
    });
  });
}

void
Controller::openFile(const QString& path) {
  QFileInfo info(path);
  if (info.isDir()) {
    openPath(path);
    return;
  }
  QProcess::startDetached("xdg-open", {path});
}
