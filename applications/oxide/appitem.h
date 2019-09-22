#ifndef APP_H
#define APP_H
#include <QObject>

class AppItem : public QObject {
    Q_OBJECT
public:

    Q_PROPERTY(QString name MEMBER _name NOTIFY nameChanged)
    Q_PROPERTY(QString desc MEMBER _desc NOTIFY descChanged)
    Q_PROPERTY(QString call MEMBER _call NOTIFY callChanged)
    Q_PROPERTY(QString term MEMBER _term NOTIFY termChanged)
    Q_PROPERTY(QString imgFile MEMBER _imgFile NOTIFY imgFileChanged)

    bool ok();

    Q_INVOKABLE void execute();
signals:
    void nameChanged();
    void descChanged();
    void callChanged();
    void termChanged();
    void imgFileChanged();

private:
    QString _name;
    QString _desc;
    QString _call;
    QString _term;
    QString _imgFile;
};
#endif // APP_H
