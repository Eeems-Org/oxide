#include "controller.h"
#include <liboxide.h>

#include <QDBusConnection>
#include <QUuid>

using namespace Oxide::Applications;
using namespace codes::eeems::oxide1;

Controller::Controller(Apps* apps, QObject* parent)
  : QObject(parent)
  , m_apps(apps) {}

void
Controller::setDefault(const QString& appPath, const QString& mimeType) {
  if (!m_apps) {
    return;
  }
  m_apps->setDefaultApplication(mimeType, QDBusObjectPath(appPath));
}

void
Controller::launch(const QString& appPath, const QString& filePath) {
  if (!m_apps) {
    return;
  }

  auto bus = QDBusConnection::systemBus();
  Application chosen(OXIDE_SERVICE, appPath, bus);
  QStringList launchArgs = {filePath};

  if (!chosen.forking()) {
    chosen.launchWithArgs(launchArgs).waitForFinished();
    return;
  }

  QStringList cloneFlags = chosen.flags();
  if (!cloneFlags.contains("transient")) {
    cloneFlags.prepend("transient");
  }
  cloneFlags.removeAll("forking");

  QVariantMap properties = {
    {"name",             QUuid::createUuid().toString(QUuid::Id128)},
    {"bin",              chosen.bin()                              },
    {"displayName",      chosen.displayName()                      },
    {"description",      chosen.description()                      },
    {"type",             (int)ApplicationType::Background          },
    {"icon",             chosen.icon()                             },
    {"workingDirectory", chosen.workingDirectory()                 },
    {"user",             chosen.user()                             },
    {"group",            chosen.group()                            },
    {"environment",      chosen.environment()                      },
    {"permissions",      chosen.permissions()                      },
    {"directories",      chosen.directories()                      },
    {"onPause",          chosen.onPause()                          },
    {"onResume",         chosen.onResume()                         },
    {"onStop",           chosen.onStop()                           },
    {"flags",            cloneFlags                                }
  };

  QDBusObjectPath clonePath = m_apps->registerApplication(properties);
  if (clonePath.path() != "/") {
    Application clone(OXIDE_SERVICE, clonePath.path(), bus);
    clone.launchWithArgs(launchArgs).waitForFinished();
  }
}
