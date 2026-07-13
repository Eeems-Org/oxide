#include "frontlightapi.h"

#include <liboxide.h>
#include <liboxide/devicesettings.h>

FrontlightAPI*
FrontlightAPI::singleton(FrontlightAPI* self) {
  static FrontlightAPI* instance;
  if (self != nullptr) {
    instance = self;
  }
  return instance;
}

FrontlightAPI::FrontlightAPI(QObject* parent)
  : APIBase(parent)
  , m_enabled(false) {
  Oxide::Sentry::sentry_transaction(
    "Frontlight API Init", "init", [this](Oxide::Sentry::Transaction* t) {
      Oxide::Sentry::sentry_span(t, "singleton", "Setup singleton", [this] {
        singleton(this);
      });
      switch (deviceSettings.getDeviceType()) {
#ifdef __aarch64__
        case Oxide::DeviceSettings::RMPP:
        case Oxide::DeviceSettings::RMPPM:
          Oxide::Sentry::sentry_span(
            t, "manager", "Create csl light manager", [this] {
              m_manager =
                new csl::light::Manager(csl::light::Source::Frontlight);
              if (!m_manager->isValid()) {
                O_WARNING("Failed to load libcsllight");
                return;
              }
              O_INFO("Frontlight brightness:" << m_manager->brightness());
            }
          );
          break;
#endif
        default:
          O_INFO("No frontlight");
          return;
      }
    }
  );
}

FrontlightAPI::~FrontlightAPI() {
#ifdef __aarch64__
  if (m_manager != nullptr) {
    delete m_manager;
  }
#endif
}

void
FrontlightAPI::setEnabled(bool enabled) {
  m_enabled = enabled;
  O_INFO("Frontlight API" << enabled);
}

bool
FrontlightAPI::getHasFrontlight() {
  return hasPermission("frontlight") && hasFrontlightNoPermissionCheck();
}

bool
FrontlightAPI::hasFrontlightNoPermissionCheck() {
#ifdef __aarch64__
  if (m_manager == nullptr || !m_manager->isValid()) {
    return false;
  }
  switch (deviceSettings.getDeviceType()) {
    case Oxide::DeviceSettings::RMPP:
    case Oxide::DeviceSettings::RMPPM:
      return true;
    default:
      return false;
  }
#else
  return false;
#endif
}

int
FrontlightAPI::getBrightness() {
  if (!hasPermission("frontlight")) {
    return 0;
  }
  return brightness();
}

int
FrontlightAPI::brightness() {
#ifdef __aarch64__
  if (!hasFrontlightNoPermissionCheck() || !m_manager->isEnabled()) {
    return 0;
  }
  return m_manager->brightness();
#else
  return 0;
#endif
}

void
FrontlightAPI::setBrightness(int brightness) {
  if (!hasPermission("frontlight")) {
    return;
  }
  setBrightnessNoPermissionCheck(brightness);
}

void
FrontlightAPI::setBrightnessNoPermissionCheck(int brightness) {
#ifdef __aarch64__
  if (!hasFrontlightNoPermissionCheck()) {
    return;
  }
  if (m_manager == nullptr || !m_manager->isValid()) {
    return;
  }
  brightness = qBound(0, brightness, 100);
  O_DEBUG("Frontlight brightness:" << brightness);
  m_manager->setEnabled(brightness > 0);
  m_manager->setBrightness(brightness);
  if (m_enabled) {
    emit brightnessChanged(brightness);
  }
#endif
}

bool
FrontlightAPI::getExtraBrightness() {
  if (!hasPermission("frontlight")) {
    return false;
  }
  return extraBrightness();
}

bool
FrontlightAPI::extraBrightness() {
#ifdef __aarch64__
  return hasFrontlightNoPermissionCheck() && m_manager != nullptr &&
         m_manager->isValid() && m_manager->isBoostedFrontlightEnabled();
#else
  return false;
#endif
}

void
FrontlightAPI::setExtraBrightness(bool enabled) {
  if (!hasPermission("frontlight")) {
    return;
  }
  setExtraBrightnessNoPermissionCheck(enabled);
}

void
FrontlightAPI::setExtraBrightnessNoPermissionCheck(bool enabled) {
#ifdef __aarch64__
  if (!hasFrontlightNoPermissionCheck()) {
    return;
  }
  O_INFO("Frontlight extra brightness:" << enabled);
  if (m_manager != nullptr && m_manager->isValid()) {
    m_manager->setBoostedFrontlight(enabled);
  }
  if (m_enabled) {
    emit extraBrightnessChanged(enabled);
  }
#endif
}

#include "moc_frontlightapi.cpp"
