#pragma once
#include <libblight/types.h>
#include <qpa/qplatformwindow.h>

#include <QScreen>

#include "oxidebackingstore.h"
#include "oxidescreen.h"

class OxideScreen;

class OxideWindow : public QPlatformWindow
{
  public:
    OxideWindow(QWindow* window);
    ~OxideWindow();

    void setBackingStore(OxideBackingStore* store);
    OxideBackingStore* backingStore() const;
    OxideScreen* platformScreen() const;
    void setGeometry(const QRect& rect) override;
    void setVisible(bool visible) override;
    void raise() override;
    void lower() override;
    bool close() override;

  protected:
    OxideBackingStore* mBackingStore;
};
