#include "appitem.h"

#include <liboxide.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include <QDebug>
#include <QUuid>
#include <fstream>
#include <iostream>

#include "controller.h"
#include "mxcfb.h"

bool
AppItem::ok() {
  return getApp() != nullptr;
}

void
AppItem::execute() {
  if (!getApp() || !app->isValid()) {
    O_WARNING("Application instance is not valid");
    return;
  }
  if (!app->forking()) {
    qDebug() << "Running application " << app->path();
    app->launch().waitForFinished();
    return;
  }
  qDebug() << "Forking new instance of" << property("name").toString();
  auto controller = reinterpret_cast<Controller*>(parent());
  auto apps = controller->getAppsApi();
  auto bus = QDBusConnection::systemBus();
  QVariantMap properties;
  properties["name"] = QUuid::createUuid().toString();
  properties["bin"] = app->bin();
  properties["displayName"] = app->displayName();
  properties["description"] = app->description();
  properties["type"] = (int)app->type();
  properties["icon"] = app->icon();
  properties["workingDirectory"] = app->workingDirectory();
  properties["user"] = app->user();
  properties["group"] = app->group();
  properties["environment"] = app->environment();
  properties["permissions"] = app->permissions();
  properties["onPause"] = app->onPause();
  properties["onResume"] = app->onResume();
  properties["onStop"] = app->onStop();
  QStringList cloneFlags = app->flags();
  if (!cloneFlags.contains("transient")) {
    cloneFlags.prepend("transient");
  }
  cloneFlags.removeAll("forking");
  properties["flags"] = cloneFlags;
  QDBusObjectPath path = apps->registerApplication(properties);
  if (path.path() == "/") {
    O_WARNING(
      "Failed to register transient instance of " << property("name").toString()
    );
    auto notifications = controller->getNotificationApi();
    notifications->add(
      QUuid::createUuid().toString(),
      "oxide",
      QStringLiteral("Failed to launch %1")
        .arg(property("displayName").toString()),
      ""
    );
    return;
  }
  Application instance(OXIDE_SERVICE, path.path(), bus);
  instance.launch().waitForFinished();
}
void
AppItem::stop() {
  if (!getApp() || !app->isValid()) {
    O_WARNING("Application instance is not valid");
    return;
  }
  app->stop();
}
Application*
AppItem::getApp() {
  if (app != nullptr) {
    return app;
  }
  auto controller = reinterpret_cast<Controller*>(parent());
  auto apps = controller->getAppsApi();
  auto bus = QDBusConnection::systemBus();
  QDBusObjectPath appPath;
  auto applications = apps->applications();
  if (!applications.contains(_name)) {
    qDebug() << "Couldn't find Application instance";
    return nullptr;
  }
  appPath = applications[_name].value<QDBusObjectPath>();
  auto instance = new Application(OXIDE_SERVICE, appPath.path(), bus, this);
  if (!instance->isValid()) {
    delete instance;
    qDebug() << "Application API instance is invalid" << instance->lastError();
    return nullptr;
  }
  connect(instance, &Application::exited, this, &AppItem::exited);
  connect(
    instance,
    &Application::displayNameChanged,
    this,
    &AppItem::onDisplayNameChanged
  );
  connect(instance, &Application::iconChanged, this, &AppItem::onIconChanged);
  connect(instance, &Application::launched, this, &AppItem::launched);

  app = instance;
  return app;
}

#include "moc_appitem.cpp"
