#pragma once
#include <QObject>

class Q_DECL_EXPORT RepaintNotifier : public QObject{
    Q_OBJECT

public:
    RepaintNotifier() : QObject(nullptr){}

signals:
    void repainted(unsigned int marker);
};
