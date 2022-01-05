#ifndef NOTIFICATIONLIST_H
#define NOTIFICATIONLIST_H

#include <QAbstractListModel>

#include "notification_interface.h"

using namespace codes::eeems::oxide1;

class NotificationItem : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString identifier MEMBER m_identifier READ identifier)
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(QString icon READ icon NOTIFY iconChanged)
    Q_PROPERTY(int created READ created NOTIFY createdChanged)
public:
    NotificationItem(Notification* notification, QObject* parent) : QObject(parent) {
        m_identifier = notification->identifier();
        m_notification = notification;
        connect(notification, &Notification::changed, this, &NotificationItem::changed);
    }
    ~NotificationItem() {
        if(m_notification != nullptr){
            delete m_notification;
        }
    }
    QString identifier() { return m_identifier; }
    QString text() {
        if(m_notification == nullptr){
            return "";
        }
        return m_notification->text();
    }
    QString icon() {
        if(m_notification == nullptr){
            return "";
        }
        return m_notification->icon();
    }
    int created(){
        if(m_notification == nullptr){
            return -1;
        }
        return m_notification->created();
    }
    bool is(Notification* notification) { return notification == m_notification; }
    Notification* notification() { return m_notification; }
    Q_INVOKABLE void click(){ notification()->click(); }
signals:
    void textChanged(QString);
    void iconChanged(QString);
    void createdChanged(int);

public slots:
    void changed(const QVariantMap& properties){
        for(auto key : properties.keys()){
            if(key == "text"){
                emit textChanged(properties.value(key, "").toString());
            }else if(key == "icon"){
                emit iconChanged(properties.value(key, "").toString());
            }
        }
    }

private:
    Notification* m_notification;
    QString m_identifier;
};


class NotificationList : public QAbstractListModel
{
    Q_OBJECT
public:
    NotificationList() : QAbstractListModel(nullptr) {}

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
        return notifications.length();
    }
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override{
        if(!index.isValid()){
            return QVariant();
        }
        if(index.column() > 0){
            return QVariant();
        }
        if(index.row() >= notifications.length()){
            return QVariant();
        }
        if(role != Qt::DisplayRole){
            return QVariant();
        }
        return QVariant::fromValue(notifications[index.row()]);
    }
    void append(Notification* notification){
        beginInsertRows(QModelIndex(), notifications.length(), notifications.length());
        notifications.append(new NotificationItem(notification, this));
        endInsertRows();
        emit updated();
    }
    Notification* get(const QDBusObjectPath& path){
        auto pathString = path.path();
        for(auto notification : notifications){
            if(pathString == notification->notification()->path()){
                return notification->notification();
            }
        }
        return nullptr;
    }
    Q_INVOKABLE void clear(){
        beginRemoveRows(QModelIndex(), 0, notifications.length());
        for(auto notification : notifications){
            notification->notification()->remove().waitForFinished();
            if(notification != nullptr){
                delete notification;
            }
        }
        notifications.clear();
        endRemoveRows();
        emit updated();
    }
    Q_INVOKABLE void remove(QString identifier){
        QMutableListIterator<NotificationItem*> i(notifications);
        while(i.hasNext()){
            auto notification = i.next();
            if(notification->identifier() == identifier){
                beginRemoveRows(QModelIndex(), notifications.indexOf(notification), notifications.indexOf(notification));
                i.remove();
                notification->notification()->remove().waitForFinished();
                delete notification;
                endRemoveRows();
            }
        }
        emit updated();
    }
    int removeAll(Notification* notification) {
        QMutableListIterator<NotificationItem*> i(notifications);
        int count = 0;
        while(i.hasNext()){
            auto item = i.next();
            if(item->is(notification)){
                beginRemoveRows(QModelIndex(), notifications.indexOf(item), notifications.indexOf(item));
                i.remove();
                item->notification()->remove().waitForFinished();
                delete item;
                endRemoveRows();
                count++;
            }
        }
        emit updated();
        return count;
    }
    int length() { return notifications.length(); }
    bool empty() { return notifications.empty(); }
signals:
    void updated();
private:
    QList<NotificationItem*> notifications;
};

#endif // NOTIFICATIONLIST_H
