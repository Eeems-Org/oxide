#include "qt.h"

QByteArray& operator>>(QByteArray& l, qint8& r){
    Q_ASSERT((size_t)l.size() >= sizeof(qint8));
    auto d = l.right(sizeof(qint8));
    l.chop(sizeof(qint8));
    memcpy(&r, d.data(), sizeof(qint8));
    return l;
}

QByteArray& operator>>(QByteArray& l, qint16& r){
    Q_ASSERT((size_t)l.size() >= sizeof(qint16));
    auto d = l.right(sizeof(qint16));
    l.chop(sizeof(qint16));
    memcpy(&r, d.data(), sizeof(qint16));
    return l;
}

QByteArray& operator>>(QByteArray& l, qint32& r){
    Q_ASSERT((size_t)l.size() >= sizeof(qint32));
    auto d = l.right(sizeof(qint32));
    l.chop(sizeof(qint32));
    memcpy(&r, d.data(), sizeof(qint32));
    return l;
}

QByteArray& operator>>(QByteArray& l, qint64& r){
    Q_ASSERT((size_t)l.size() >= sizeof(qint64));
    auto d = l.right(sizeof(qint64));
    l.chop(sizeof(qint64));
    memcpy(&r, d.data(), sizeof(qint64));
    return l;
}

QByteArray& operator>>(QByteArray& l, quint8& r){
    Q_ASSERT((size_t)l.size() >= sizeof(quint8));
    auto d = l.right(sizeof(quint8));
    l.chop(sizeof(quint8));
    memcpy(&r, d.data(), sizeof(quint8));
    return l;
}

QByteArray& operator>>(QByteArray& l, quint16& r){
    Q_ASSERT((size_t)l.size() >= sizeof(quint16));
    auto d = l.right(sizeof(quint16));
    l.chop(sizeof(quint16));
    memcpy(&r, d.data(), sizeof(quint16));
    return l;
}

QByteArray& operator>>(QByteArray& l, quint32& r){
    Q_ASSERT((size_t)l.size() >= sizeof(quint32));
    auto d = l.right(sizeof(quint32));
    l.chop(sizeof(quint32));
    memcpy(&r, d.data(), sizeof(quint32));
    return l;
}

QByteArray& operator>>(QByteArray& l, quint64& r){
    Q_ASSERT((size_t)l.size() >= sizeof(quint64));
    auto d = l.right(sizeof(quint64));
    l.chop(sizeof(quint64));
    memcpy(&r, d.data(), sizeof(quint64));
    return l;
}

QByteArray& operator<<(QByteArray& l, qint8 r){
    l.append((const char*)&r, sizeof(qint8));
    return l;
}

QByteArray& operator<<(QByteArray& l, qint16 r){
    l.append((const char*)&r, sizeof(qint16));
    return l;
}

QByteArray& operator<<(QByteArray& l, qint32 r){
    l.append((const char*)&r, sizeof(qint32));
    return l;
}

QByteArray& operator<<(QByteArray& l, qint64 r){
    l.append((const char*)&r, sizeof(qint64));
    return l;
}

QByteArray& operator<<(QByteArray& l, quint8 r){
    l.append((const char*)&r, sizeof(quint8));
    return l;
}

QByteArray& operator<<(QByteArray& l, quint16 r){
    l.append((const char*)&r, sizeof(quint16));
    return l;
}

QByteArray& operator<<(QByteArray& l, quint32 r){
    l.append((const char*)&r, sizeof(quint32));
    return l;
}

QByteArray& operator<<(QByteArray& l, quint64 r){
    l.append((const char*)&r, sizeof(quint64));
    return l;
}
