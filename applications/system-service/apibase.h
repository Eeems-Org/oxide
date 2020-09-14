#ifndef APIBASE_H
#define APIBASE_H

#include <QObject>

#include "dbussettings.h"

class APIBase : public QObject{
    Q_OBJECT
    Q_CLASSINFO("Version", OXIDE_INTERFACE_VERSION)
public:
    APIBase(QObject* parent) : QObject(parent) {}
    virtual void setEnabled(bool enabled) = 0;
};

#endif // APIBASE_H
