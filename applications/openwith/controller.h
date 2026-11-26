#pragma once

#include <liboxide.h>

#include <QObject>

class Controller : public QObject {
  Q_OBJECT

public:
  explicit Controller(
    codes::eeems::oxide1::Apps* apps,
    QObject* parent = nullptr
  );

  Q_INVOKABLE void launch(const QString& appPath, const QString& filePath);
  Q_INVOKABLE void setDefault(const QString& appPath, const QString& mimeType);

private:
  codes::eeems::oxide1::Apps* m_apps;
};
