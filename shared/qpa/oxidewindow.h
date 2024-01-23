#pragma once
#include "oxidebackingstore.h"
#include "oxidescreen.h"

#include <QScreen>
#include <qpa/qplatformwindow.h>
#include <libblight/types.h>>

class OxideScreen;

class OxideWindow : public QPlatformWindow {
public:
    OxideWindow(QWindow* window);
    ~OxideWindow();

    void setBackingStore(OxideBackingStore* store);
    OxideBackingStore* backingStore() const;
    OxideScreen* platformScreen() const;
    void setVisible(bool visible) override;
    virtual void repaint(const QRegion& region);
    bool close() override;
    void raise() override;
    void lower() override;
    bool isTopWindow();

protected:
    OxideBackingStore* mBackingStore;
    QRect mOldGeometry;
};
