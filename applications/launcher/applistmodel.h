#ifndef APPLISTMODEL_H
#define APPLISTMODEL_H

#include <QAbstractListModel>
#include <QFile>
#include <QSet>
#include <QVariant>

#include "appitem.h"

class AppListModel : public QAbstractListModel {
  Q_OBJECT

public:
  AppListModel(QObject* parent = nullptr)
    : QAbstractListModel(parent) {}

  QVariant headerData(
    int section,
    Qt::Orientation orientation,
    int role = Qt::DisplayRole
  ) const override {
    Q_UNUSED(section)
    Q_UNUSED(orientation)
    Q_UNUSED(role)
    return QVariant();
  }
  int rowCount(const QModelIndex& parent = QModelIndex()) const override {
    if (parent.isValid()) {
      return 0;
    }
    return apps.length();
  }
  QVariant
  data(const QModelIndex& index, int role = Qt::DisplayRole) const override {
    if (!index.isValid()) {
      return QVariant();
    }
    if (index.column() > 0) {
      return QVariant();
    }
    if (index.row() >= apps.length()) {
      return QVariant();
    }
    if (role != Qt::DisplayRole) {
      return QVariant();
    }
    return QVariant::fromValue(apps[index.row()]);
  }
  void append(AppItem* app) {
    beginInsertRows(QModelIndex(), apps.length(), apps.length());
    apps.append(app);
    endInsertRows();
  }
  void removeAt(int index) {
    beginRemoveRows(QModelIndex(), index, index);
    auto app = apps.takeAt(index);
    app->deleteLater();
    endRemoveRows();
  }
  AppItem* findByName(const QString& name) {
    for (auto app : apps) {
      if (app->property("name").toString() == name) {
        return app;
      }
    }
    return nullptr;
  }
  Q_INVOKABLE int length() { return apps.length(); }
  Q_INVOKABLE bool empty() { return apps.empty(); }

  void update(
    const QString& serviceName,
    QObject* parent,
    const QVariantMap& registered,
    const QVariantMap& running
  ) {
    auto bus = QDBusConnection::systemBus();
    // Track which names are still registered
    QSet<QString> seen;
    bool changed = false;
    // Add or update items
    for (auto it = registered.constBegin(); it != registered.constEnd(); ++it) {
      auto name = it.key();
      auto path = it.value().value<QDBusObjectPath>().path();
      seen.insert(name);
      auto appItem = findByName(name);
      if (appItem != nullptr) {
        // Existing app — only update running state, no D-Bus calls
        auto isRunning = running.contains(name);
        if (appItem->property("running") != isRunning) {
          appItem->setProperty("running", isRunning);
        }
        continue;
      }
      // New app — need D-Bus proxy to read properties
      Application app(serviceName, path, bus, parent);
      if (app.hidden() || app.transient()) {
        continue;
      }
      qDebug() << "New application:" << name;
      auto displayName = app.displayName();
      if (displayName.isEmpty()) {
        displayName = name;
      }
      auto desc = app.description();
      auto call = app.bin();
      auto isRunning = running.contains(name);
      auto icon = app.icon();
      auto imgFile =
        (!icon.isEmpty() && QFile(icon).exists()) ? "file:" + icon : "";
      appItem = new AppItem(parent);
      appItem->setProperty("path", path);
      appItem->setProperty("name", name);
      appItem->setProperty("displayName", displayName);
      appItem->setProperty("desc", desc);
      appItem->setProperty("call", call);
      appItem->setProperty("running", isRunning);
      appItem->setProperty("imgFile", imgFile);
      if (!appItem->ok()) {
        qDebug() << "Invalid item" << name;
        appItem->deleteLater();
        continue;
      }
      append(appItem);
      changed = true;
    }
    // Remove items no longer registered
    QMutableListIterator<AppItem*> i(apps);
    while (i.hasNext()) {
      auto appItem = i.next();
      auto name = appItem->property("name").toString();
      if (!seen.contains(name)) {
        auto idx = apps.indexOf(appItem);
        if (idx >= 0) {
          removeAt(idx);
          changed = true;
        }
      }
    }
    if (!changed) {
      return;
    }
    // Sort by displayName
    auto oldOrder = apps;
    std::sort(
      apps.begin(), apps.end(), [](const AppItem* a, const AppItem* b) -> bool {
        return a->property("displayName").toString() <
               b->property("displayName").toString();
      }
    );
    if (apps != oldOrder) {
      emit layoutChanged();
    }
  }

signals:
  void updated();

private:
  QList<AppItem*> apps;
};

#endif // APPLISTMODEL_H
