#pragma once
#include <QObject>

class Window;

class RepaintNotifier : public QObject{
    Q_OBJECT

signals:
    void windowRepainted(Window* window, unsigned int marker);
    void repainted(unsigned int marker);
};
