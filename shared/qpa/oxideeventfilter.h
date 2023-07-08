#pragma once

#include <QObject>
#include <QPointF>

class OxideEventFilter : public QObject{
    Q_OBJECT

public:
    OxideEventFilter(QObject* parent);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    static QPointF transpose(QPointF pointF);
    static QPointF swap(QPointF pointF);
};
