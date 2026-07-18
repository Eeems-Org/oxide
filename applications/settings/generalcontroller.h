#pragma once

#include <QObject>
#include <QStringList>

#include <liboxide.h>

using namespace Oxide;

class GeneralController : public QObject {
  Q_OBJECT
  Q_PROPERTY(QStringList locales READ locales NOTIFY localesChanged)
  Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
  Q_PROPERTY(QStringList timezones READ timezones NOTIFY timezonesChanged)
  Q_PROPERTY(
    QString timezone READ timezone WRITE setTimezone NOTIFY timezoneChanged
  )

public:
  explicit GeneralController(QObject* parent = nullptr)
    : QObject(parent) {}

  QStringList locales() { return deviceSettings.getLocales(); }
  QString locale() { return deviceSettings.getLocale(); }
  void setLocale(const QString& locale) {
    deviceSettings.setLocale(locale);
    emit localeChanged(locale);
  }
  QStringList timezones() { return deviceSettings.getTimezones(); }
  QString timezone() { return deviceSettings.getTimezone(); }
  void setTimezone(const QString& timezone) {
    deviceSettings.setTimezone(timezone);
    emit timezoneChanged(timezone);
  }

signals:
  void localesChanged();
  void localeChanged(QString);
  void timezonesChanged();
  void timezoneChanged(QString);
};
