#ifndef GUITHREAD_H
#define GUITHREAD_H

#include <QThread>

class GuiThread : public QThread {
public:
    explicit GuiThread(QObject *parent = nullptr);
};

#endif // GUITHREAD_H
