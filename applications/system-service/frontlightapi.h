#ifndef FRONTLIGHTAPI_H
#define FRONTLIGHTAPI_H

#include "csl_light.h"
#include <liboxide.h>

#include <QObject>

#include "apibase.h"

#define frontlightAPI FrontlightAPI::singleton()

class FrontlightAPI : public APIBase {
  Q_OBJECT
  Q_CLASSINFO("D-Bus Interface", OXIDE_FRONTLIGHT_INTERFACE)
  Q_PROPERTY(
    int brightness READ getBrightness WRITE setBrightness NOTIFY
      brightnessChanged
  )
  Q_PROPERTY(bool hasFrontlight READ getHasFrontlight)
  Q_PROPERTY(
    bool extraBrightness READ getExtraBrightness WRITE setExtraBrightness NOTIFY
      extraBrightnessChanged
  )

public:
  static FrontlightAPI* singleton(FrontlightAPI* self = nullptr);
  FrontlightAPI(QObject* parent);
  ~FrontlightAPI();
  void setEnabled(bool enabled) override;

  int getBrightness();
  int brightness();
  void setBrightness(int brightness);
  void setBrightnessNoPermissionCheck(int brightness);
  bool getHasFrontlight();
  bool hasFrontlightNoPermissionCheck();
  bool getExtraBrightness();
  bool extraBrightness();
  void setExtraBrightness(bool enabled);
  void setExtraBrightnessNoPermissionCheck(bool enabled);

signals:
  void brightnessChanged(int);
  void extraBrightnessChanged(bool);

private:
  bool m_enabled = false;
#ifdef __aarch64__
  csl::light::Manager* m_manager = nullptr;
#endif
};

#endif // FRONTLIGHTAPI_H
