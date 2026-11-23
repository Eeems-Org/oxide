#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class Controller : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
  Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
  Q_PROPERTY(QString homePath READ homePath CONSTANT)

public:
  explicit Controller(QObject* parent = nullptr);

  QString currentPath() const;
  QVariantList entries() const;
  QString status() const;
  bool busy() const;
  QString homePath() const;

  Q_INVOKABLE void openPath(const QString& path);
  Q_INVOKABLE void goUp();
  Q_INVOKABLE void goHome();
  Q_INVOKABLE void refresh();
  Q_INVOKABLE void createFolder(const QString& name);
  Q_INVOKABLE void renamePath(const QString& oldPath, const QString& newName);
  Q_INVOKABLE void removePaths(const QStringList& paths);
  Q_INVOKABLE void
  copyPaths(const QStringList& paths, const QString& destination);
  Q_INVOKABLE void
  movePaths(const QStringList& paths, const QString& destination);
  Q_INVOKABLE void openFile(const QString& path);

signals:
  void currentPathChanged();
  void entriesChanged();
  void statusChanged();
  void busyChanged();

private:
  void loadDirectory();
  void clearStatus();
  void setBusy(bool busy);
  bool copyRecursive(const QString& source, const QString& destination);

  QString m_currentPath;
  QVariantList m_entries;
  QString m_status;
  bool m_busy = false;
};