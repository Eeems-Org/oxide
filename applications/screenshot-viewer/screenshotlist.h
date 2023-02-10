#ifndef SCREENSHOTLIST_H
#define SCREENSHOTLIST_H

#include <QAbstractListModel>
#include <liboxide/dbus.h>

using namespace codes::eeems::oxide1;

class ScreenshotItem : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)

public:
    ScreenshotItem(Screenshot* screenshot, QObject* parent) : QObject(parent) {
        m_screenshot = screenshot;
        connect(screenshot, &Screenshot::modified, this, &ScreenshotItem::modified);
    }
    ~ScreenshotItem() {
        if(m_screenshot != nullptr){
            delete m_screenshot;
        }
    }
    QString path() {
        if(m_screenshot == nullptr){
            return "";
        }
        return m_screenshot->path();
    }
    QString name() { return QFileInfo(path()).baseName(); }
    bool is(Screenshot* screenshot) { return screenshot == m_screenshot; }
    Screenshot* screenshot() { return m_screenshot; }
    bool remove() {
        auto reply = m_screenshot->remove();
        reply.waitForFinished();
        if(reply.isError() || !reply.isValid()){
            qDebug() << reply.error();
            return false;
        }
        return true;
    }
    QString qPath() { return m_screenshot->QDBusAbstractInterface::path(); }

signals:
    void pathChanged(QString);
    void nameChanged(QString);

public slots:
    void modified(){
        emit pathChanged(path());
        emit nameChanged(name());
    }

private:
    Screenshot* m_screenshot;
};


class ScreenshotList : public QAbstractListModel
{
    Q_OBJECT

public:
    ScreenshotList() : QAbstractListModel(nullptr) {}

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override {
        Q_UNUSED(section)
        Q_UNUSED(orientation)
        Q_UNUSED(role)
        return QVariant();
    }
    int rowCount(const QModelIndex& parent = QModelIndex()) const override{
        if(parent.isValid()){
            return 0;
        }
        return screenshots.length();
    }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override{
        if(!index.isValid()){
            return QVariant();
        }
        if(index.column() > 0){
            return QVariant();
        }
        if(index.row() >= screenshots.length()){
            return QVariant();
        }
        if(role != Qt::DisplayRole){
            return QVariant();
        }
        return QVariant::fromValue(screenshots[index.row()]);
    }
    void append(Screenshot* screenshot){
        beginInsertRows(QModelIndex(), screenshots.length(), screenshots.length());
        screenshots.append(new ScreenshotItem(screenshot, this));
        endInsertRows();
        emit updated();
    }
    ScreenshotItem* get(const QDBusObjectPath& path){
        auto pathString = path.path();
        for(auto screenshot : screenshots){
            if(pathString == screenshot->qPath()){
                return screenshot;
            }
        }
        return nullptr;
    }
    Q_INVOKABLE void clear(){
        beginRemoveRows(QModelIndex(), 0, screenshots.length());
        for(auto screenshot : screenshots){
            screenshot->remove();
            delete screenshot;
        }
        screenshots.clear();
        endRemoveRows();
        emit updated();
    }
    Q_INVOKABLE void remove(QString path){
        QMutableListIterator<ScreenshotItem*> i(screenshots);
        while(i.hasNext()){
            auto screenshot = i.next();
            if(screenshot->path() == path){
                beginRemoveRows(QModelIndex(), screenshots.indexOf(screenshot), screenshots.indexOf(screenshot));
                i.remove();
                screenshot->remove();
                delete screenshot;
                endRemoveRows();
            }
        }
        emit updated();
    }
    void remove(const QDBusObjectPath& path){
        auto screenshot = get(path);
        if(screenshot != nullptr){
            remove(screenshot->path());
        }
    }
    int removeAll(Screenshot* screenshot) {
        QMutableListIterator<ScreenshotItem*> i(screenshots);
        int count = 0;
        while(i.hasNext()){
            auto item = i.next();
            if(item->is(screenshot) && item->remove()){
                beginRemoveRows(QModelIndex(), screenshots.indexOf(item), screenshots.indexOf(item));
                i.remove();
                delete item;
                endRemoveRows();
                count++;
            }
        }
        emit updated();
        return count;
    }
    int length() { return screenshots.length(); }
    bool empty() { return screenshots.empty(); }

signals:
    void updated();

private:
    QList<ScreenshotItem*> screenshots;
};

#endif // SCREENSHOTLIST_H
