#pragma once

#include <liboxide/debug.h>
#include <liboxide/devicesettings.h>
#include <liboxide/oxideqml.h>

#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QKeyEvent>
#include <QList>
#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <QUrl>

#include <cerrno>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <unistd.h>

#include "application.h"
#include "appsapi.h"
#include "dbusservice.h"
#include "eventlistener.h"
#include "screenapi.h"
#include "systemapi.h"

class Controller : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString deviceName READ deviceName)
  Q_PROPERTY(
    bool leftSwipeEnabled READ leftSwipeEnabled NOTIFY leftSwipeEnabledChanged
  )
  Q_PROPERTY(
    bool rightSwipeEnabled READ rightSwipeEnabled NOTIFY
      rightSwipeEnabledChanged
  )
  Q_PROPERTY(
    bool upSwipeEnabled READ upSwipeEnabled NOTIFY upSwipeEnabledChanged
  )
  Q_PROPERTY(
    bool downSwipeEnabled READ downSwipeEnabled NOTIFY downSwipeEnabledChanged
  )
  Q_PROPERTY(
    int leftSwipeLength READ leftSwipeLength NOTIFY leftSwipeLengthChanged
  )
  Q_PROPERTY(
    int rightSwipeLength READ rightSwipeLength NOTIFY rightSwipeLengthChanged
  )
  Q_PROPERTY(int upSwipeLength READ upSwipeLength NOTIFY upSwipeLengthChanged)
  Q_PROPERTY(
    int downSwipeLength READ downSwipeLength NOTIFY downSwipeLengthChanged
  )
  Q_PROPERTY(bool lidOpen READ lidOpen NOTIFY lidOpenChanged)

public:
  static Controller* singleton() {
    static Controller* instance = new Controller(qApp);
    return instance;
  }
  bool enabled = true; // Used to disable sending input when suspending
  Q_INVOKABLE void screenshot() { screenAPI->screenshot(); }
  Q_INVOKABLE void taskSwitcher() { appsAPI->openTaskSwitcher(); }
  Q_INVOKABLE void processManager() { appsAPI->openTaskManager(); }
  Q_INVOKABLE void back() {
    auto currentApplication =
      appsAPI->getApplication(appsAPI->currentApplicationNoSecurityCheck());
    if (
      currentApplication != nullptr &&
      currentApplication->path() == appsAPI->lockscreenApplication().path()
    ) {
      O_DEBUG("Back cancelled. On lockscreen");
      return;
    }
    if (!appsAPI->previousApplicationNoSecurityCheck()) {
      appsAPI->openDefaultApplication();
    }
  }
  Q_INVOKABLE void lock() { appsAPI->openLockScreen(); }
  Q_INVOKABLE void terminal() { appsAPI->openTerminal(); }
  Q_INVOKABLE void close() {
    auto path = appsAPI->currentApplicationNoSecurityCheck();
    if (path.path() == "/") {
      return;
    }
    if (path == appsAPI->lockscreenApplication()) {
      return;
    }
    if (path == appsAPI->startupApplication()) {
      return;
    }
    if (path == appsAPI->taskSwitcherApplication()) {
      return;
    }
    auto currentApplication = appsAPI->getApplication(path);
    if (
      currentApplication == nullptr ||
      currentApplication->stateNoSecurityCheck() == Application::Inactive
    ) {
      return;
    }
    currentApplication->stop();
  }
  Q_INVOKABLE void toggleSwipes() { systemAPI->toggleSwipes(); }
  Q_INVOKABLE void suspend() { systemAPI->suspend(); }
  Q_INVOKABLE void powerOff() { systemAPI->powerOff(); }
  Q_INVOKABLE void reboot() { systemAPI->reboot(); }
  Q_INVOKABLE void pauseApplication(const QString& name, bool startIfNone) {
    auto app = appsAPI->getApplication(name);
    if (app != nullptr) {
      app->pauseNoSecurityCheck(startIfNone);
    }
  }
  Q_INVOKABLE void resumeApplication(const QString& name) {
    auto app = appsAPI->getApplication(name);
    if (app != nullptr) {
      app->resumeNoSecurityCheck();
    }
  }
  Q_INVOKABLE QString currentApplication() {
    auto app = getCurrentApplication();
    return app == nullptr ? "" : app->name();
  }
  Q_INVOKABLE QString lockscreenApplication() {
    auto path = appsAPI->lockscreenApplication();
    auto app = path.path() == "/" ? nullptr : appsAPI->getApplication(path);
    return app == nullptr ? "" : app->name();
  }
  Q_INVOKABLE QObject* loadQML(QUrl url) {
    auto* object = Oxide::QML::loadQML(dbusService->engine(), url);
    if (object == nullptr) {
      return nullptr;
    }
    QWindow* window = qobject_cast<QQuickWindow*>(object);
    if (window == nullptr) {
      return object;
    }
    auto buffer = Oxide::QML::getSurfaceForWindow(window);
    if (buffer == nullptr) {
      return object;
    }
    getCompositorDBus()->setFlags(
      QString("connection/%1/surface/%2").arg(getpid()).arg(buffer->surface),
      QStringList() << "system"
    );
    return object;
  }

  QString deviceName() { return deviceSettings.getDeviceName(); }
  bool leftSwipeEnabled() {
    return systemAPI->getSwipeEnabled(SystemAPI::Left);
  }
  bool rightSwipeEnabled() {
    return systemAPI->getSwipeEnabled(SystemAPI::Right);
  }
  bool upSwipeEnabled() { return systemAPI->getSwipeEnabled(SystemAPI::Up); }
  bool downSwipeEnabled() {
    return systemAPI->getSwipeEnabled(SystemAPI::Down);
  }
  int leftSwipeLength() { return systemAPI->getSwipeLength(SystemAPI::Left); }
  int rightSwipeLength() { return systemAPI->getSwipeLength(SystemAPI::Right); }
  int upSwipeLength() { return systemAPI->getSwipeLength(SystemAPI::Up); }
  int downSwipeLength() { return systemAPI->getSwipeLength(SystemAPI::Down); }
  bool lidOpen() {
    Manager* systemd = systemAPI->systemdManager();
    return systemd == nullptr || !systemd->lidClosed();
  }
  bool shouldResume() {
    auto* engine = dbusService->engine();
    if (engine == nullptr) {
      return true;
    }
    auto* rootObject = engine->rootObjects().value(0);
    if (rootObject == nullptr) {
      return true;
    }
    QVariant result;
    if (
      QMetaObject::invokeMethod(
        rootObject, "shouldResume", Q_RETURN_ARG(QVariant, result)
      )
    ) {
      return result.toBool();
    }
    return true;
  }

  ~Controller() {
    for (auto& monitor : m_switchMonitors) {
      delete monitor.notifier;
      if (monitor.fd >= 0) {
        ::close(monitor.fd);
      }
    }
  }

signals:
  void keyPressed(int, int, const QString&);
  void keyReleased(int, int, const QString&);
  void leftSwipeEnabledChanged(bool);
  void rightSwipeEnabledChanged(bool);
  void upSwipeEnabledChanged(bool);
  void downSwipeEnabledChanged(bool);
  void leftSwipeLengthChanged(int);
  void rightSwipeLengthChanged(int);
  void upSwipeLengthChanged(int);
  void downSwipeLengthChanged(int);
  void lidClosed();
  void lidOpened();
  void penAttached();
  void penDetached();
  void lidOpenChanged(bool open);

private slots:
  void debouncedReload(const QString& path) {
    Q_UNUSED(path);
    m_reloadTimer.start();
  }
  void reload() {
    updateFileWatches();
    dbusService->reload();
  }

private:
  struct SwitchMonitor {
    int fd = -1;
    QSocketNotifier* notifier = nullptr;
  };
  QList<SwitchMonitor> m_switchMonitors;
  QFileSystemWatcher m_fileWatcher;
  QTimer m_reloadTimer;

  void readEvents() {
    auto notifier = qobject_cast<QSocketNotifier*>(sender());
    if (notifier == nullptr) {
      return;
    }
    int fd = notifier->socket();
    struct input_event ev;
    int lidState = -1;
    int penState = -1;
    while (true) {
      auto res = ::read(fd, &ev, sizeof(ev));
      if (res == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
          break;
        }
        O_WARNING("Failed reading event device, disabling it");
        for (int i = 0; i < m_switchMonitors.size(); i++) {
          auto& monitor = m_switchMonitors[i];
          if (monitor.fd != fd) {
            continue;
          }
          monitor.notifier->setEnabled(false);
          monitor.notifier->deleteLater();
          ::close(m_switchMonitors[i].fd);
          m_switchMonitors.removeAt(i);
          break;
        }
        return;
      }
      if (res != sizeof(ev)) {
        break;
      }
      if (ev.type == EV_SW) {
        switch (ev.code) {
          case SW_LID:
            lidState = ev.value;
            break;
          case SW_PEN_INSERTED:
            penState = ev.value;
            break;
        }
        continue;
      }
      if (ev.type != EV_SYN) {
        continue;
      }
      if (ev.code == SYN_DROPPED) {
        lidState = -1;
        penState = -1;
        continue;
      }
      if (lidState != -1) {
        if (lidState) {
          emit lidOpenChanged(false);
          emit lidClosed();
        } else {
          emit lidOpenChanged(true);
          emit lidOpened();
        }
      }
      if (penState != -1) {
        if (penState) {
          emit penAttached();
        } else {
          emit penDetached();
        }
      }
      lidState = -1;
      penState = -1;
    }
  }

  void inputDevicesChanged() {
    for (auto& monitor : m_switchMonitors) {
      delete monitor.notifier;
      if (monitor.fd >= 0) {
        ::close(monitor.fd);
      }
    }
    m_switchMonitors.clear();
    for (auto& sw : deviceSettings.switches()) {
      int fd = ::open(sw.device.c_str(), O_RDONLY | O_NONBLOCK);
      if (fd < 0) {
        continue;
      }
      auto notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
      connect(
        notifier, &QSocketNotifier::activated, this, &Controller::readEvents
      );
      m_switchMonitors.append({fd, notifier});
    }
  }

  Application* getCurrentApplication() {
    auto path = appsAPI->currentApplicationNoSecurityCheck();
    return path.path() == "/" ? nullptr : appsAPI->getApplication(path);
  }

  void updateFileWatches() {
    QStringList files;
    QStringList directories;
    for (auto& path :
         {sharedSettings.systemOverlay(),
          sharedSettings.notificationOverlay()}) {
      auto url =
        QUrl::fromUserInput(path, QDir::currentPath(), QUrl::AssumeLocalFile);
      if (url.scheme().isEmpty()) {
        url.setScheme("file");
      }
      if (!url.isLocalFile()) {
        continue;
      }
      QFileInfo info(url.toLocalFile());
      if (info.exists()) {
        files.append(info.filePath());
      }
      if (info.dir().exists()) {
        directories.append(info.dir().path());
      }
    }
    for (auto& path : m_fileWatcher.files()) {
      if (!files.contains(path)) {
        m_fileWatcher.removePath(path);
      }
    }
    for (auto& path : m_fileWatcher.directories()) {
      if (!directories.contains(path)) {
        m_fileWatcher.removePath(path);
      }
    }
    for (auto& path : files) {
      if (!m_fileWatcher.files().contains(path)) {
        m_fileWatcher.addPath(path);
      }
    }
    for (auto& path : directories) {
      if (!m_fileWatcher.directories().contains(path)) {
        m_fileWatcher.addPath(path);
      }
    }
  }

  explicit Controller(QObject* parent = nullptr)
    : QObject(parent)
    , m_fileWatcher{}
    , m_reloadTimer{} {
    connect(
      systemAPI,
      &SystemAPI::swipeLengthChanged,
      this,
      [this](int swipe, int length) {
        switch ((SystemAPI::SwipeDirection)swipe) {
          case SystemAPI::Left:
            emit leftSwipeLengthChanged(length);
            break;
          case SystemAPI::Right:
            emit rightSwipeLengthChanged(length);
            break;
          case SystemAPI::Up:
            emit upSwipeLengthChanged(length);
            break;
          case SystemAPI::Down:
            emit downSwipeLengthChanged(length);
            break;
          default:
            break;
        }
      }
    );
    connect(
      systemAPI,
      &SystemAPI::swipeEnabledChanged,
      this,
      [this](int swipe, bool enabled) {
        switch ((SystemAPI::SwipeDirection)swipe) {
          case SystemAPI::Left:
            emit leftSwipeEnabledChanged(enabled);
            break;
          case SystemAPI::Right:
            emit rightSwipeEnabledChanged(enabled);
            break;
          case SystemAPI::Up:
            emit upSwipeEnabledChanged(enabled);
            break;
          case SystemAPI::Down:
            emit downSwipeEnabledChanged(enabled);
            break;
          default:
            break;
        }
      }
    );
    eventListener->append([this](QObject* object, QEvent* event) {
      if (!enabled) {
        return false;
      }
      Q_UNUSED(object);
      switch (event->type()) {
        case QEvent::KeyPress: {
          auto keyEvent = static_cast<QKeyEvent*>(event);
          emit keyPressed(
            keyEvent->key(), keyEvent->modifiers(), keyEvent->text()
          );
          return true;
        }
        case QEvent::KeyRelease: {
          auto keyEvent = static_cast<QKeyEvent*>(event);
          emit keyReleased(
            keyEvent->key(), keyEvent->modifiers(), keyEvent->text()
          );
          return true;
        }
        default:
          break;
      }
      return false;
    });
    inputDevicesChanged();
    deviceSettings.onInputDevicesChanged([this] { inputDevicesChanged(); });
    m_reloadTimer.setSingleShot(true);
    m_reloadTimer.setInterval(200);
    connect(
      &sharedSettings,
      &Oxide::SharedSettings::systemOverlayChanged,
      this,
      &Controller::debouncedReload
    );
    connect(
      &sharedSettings,
      &Oxide::SharedSettings::notificationOverlayChanged,
      this,
      &Controller::debouncedReload
    );
    connect(
      &m_fileWatcher,
      &QFileSystemWatcher::fileChanged,
      this,
      &Controller::debouncedReload
    );
    connect(
      &m_fileWatcher,
      &QFileSystemWatcher::directoryChanged,
      this,
      &Controller::debouncedReload
    );
    connect(&m_reloadTimer, &QTimer::timeout, this, &Controller::reload);
    updateFileWatches();
  }
};
