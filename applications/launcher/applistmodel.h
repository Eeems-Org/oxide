#ifndef APPLISTMODEL_H
#define APPLISTMODEL_H

#include <QAbstractListModel>
#include <QFile>
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
  AppItem* findByName(const QString& name) {
    for (auto app : apps) {
      if (app->property("name").toString() == name) {
        return app;
      }
    }
    return nullptr;
  }
  void update(
    const QString& serviceName,
    QObject* parent,
    const QVariantMap& registered,
    const QVariantMap& running
  ) {
    auto bus = QDBusConnection::systemBus();
    QList<AppItem*> newItems;
    for (auto it = registered.constBegin(); it != registered.constEnd(); ++it) {
      auto name = it.key();
      auto path = it.value().value<QDBusObjectPath>().path();
      Application app(serviceName, path, bus, parent);
      if (app.hidden() || app.transient()) {
        continue;
      }
      auto displayName = app.displayName();
      if (displayName.isEmpty()) {
        displayName = name;
      }
      auto desc = app.description();
      auto call = app.bin();
      auto isRunning = running.contains(name);
      auto icon = app.icon();
      auto appItem = findByName(name);
      if (appItem != nullptr) {
        appItem->update(path, displayName, desc, call, isRunning, icon);
        continue;
      }
      appItem = new AppItem(parent);
      appItem->setProperty("name", name);
      appItem->update(path, displayName, desc, call, isRunning, icon);
      if (!appItem->ok()) {
        qDebug() << "Invalid item" << name;
        appItem->deleteLater();
        continue;
      }
      newItems.append(appItem);
    }
    for (auto appItem : newItems) {
      auto pos = std::lower_bound(
        apps.begin(), apps.end(), appItem, [](AppItem* a, AppItem* b) {
          return a->property("displayName").toString() <
                 b->property("displayName").toString();
        }
      );
      auto idx = pos - apps.begin();
      beginInsertRows(QModelIndex(), idx, idx);
      apps.insert(pos, appItem);
      endInsertRows();
    }
    for (int i = apps.size() - 1; i >= 0; --i) {
      if (!registered.contains(apps[i]->property("name").toString())) {
        beginRemoveRows(QModelIndex(), i, i);
        apps.takeAt(i)->deleteLater();
        endRemoveRows();
      }
    }
  }

private:
  QList<AppItem*> apps;
};

#endif // APPLISTMODEL_H
