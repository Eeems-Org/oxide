#pragma once
#include "oxidebackingstore.h"
#include "oxidescreen.h"

#include <QScreen>
#include <qpa/qplatformwindow.h>

class OxideScreen;

class OxideWindow : public QPlatformWindow {
public:
    OxideWindow(QWindow* window) : QPlatformWindow(window), mBackingStore(nullptr){ }
    ~OxideWindow(){ }

    void setBackingStore(OxideBackingStore* store) { mBackingStore = store; }
    OxideBackingStore* backingStore() const { return mBackingStore; }
    OxideScreen* platformScreen() const;
    void setVisible(bool visible) override;
    virtual void repaint(const QRegion& region);
    bool close() override;
    void raise() override;
    void lower() override;
    void setGeometry(const QRect &rect) override;

protected:
    OxideBackingStore* mBackingStore;
    QRect mOldGeometry;
};
