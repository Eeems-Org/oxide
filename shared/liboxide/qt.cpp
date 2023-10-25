#include "qt.h"
QByteArray& operator>>(QByteArray& l, qint8& r){
    Q_ASSERT((size_t)l.size() >= sizeof(qint8));
    auto d = l.left(sizeof(qint8));
    memcpy(&r, d.data(), sizeof(qint8));
    l = l.mid(sizeof(qint8));
    return l;
}

QByteArray& operator>>(QByteArray& l, qint16& r){
    Q_ASSERT((size_t)l.size() >= sizeof(qint16));
    auto d = l.left(sizeof(qint16));
    memcpy(&r, d.data(), sizeof(qint16));
    l = l.mid(sizeof(qint16));
    return l;
}

QByteArray& operator>>(QByteArray& l, qint32& r){
    Q_ASSERT((size_t)l.size() >= sizeof(qint32));
    auto d = l.left(sizeof(qint32));
    memcpy(&r, d.data(), sizeof(qint32));
    l = l.mid(sizeof(qint32));
    return l;
}

QByteArray& operator>>(QByteArray& l, qint64& r){
    Q_ASSERT((size_t)l.size() >= sizeof(qint64));
    auto d = l.left(sizeof(qint64));
    memcpy(&r, d.data(), sizeof(qint64));
    l = l.mid(sizeof(qint64));
    return l;
}

QByteArray& operator>>(QByteArray& l, quint8& r){
    Q_ASSERT((size_t)l.size() >= sizeof(quint8));
    auto d = l.left(sizeof(quint8));
    memcpy(&r, d.data(), sizeof(quint8));
    l = l.mid(sizeof(quint8));
    return l;
}

QByteArray& operator>>(QByteArray& l, quint16& r){
    Q_ASSERT((size_t)l.size() >= sizeof(quint16));
    auto d = l.left(sizeof(quint16));
    memcpy(&r, d.data(), sizeof(quint16));
    l = l.mid(sizeof(quint16));
    return l;
}

QByteArray& operator>>(QByteArray& l, quint32& r){
    Q_ASSERT((size_t)l.size() >= sizeof(quint32));
    auto d = l.left(sizeof(quint32));
    memcpy(&r, d.data(), sizeof(quint32));
    l = l.mid(sizeof(quint32));
    return l;
}

QByteArray& operator>>(QByteArray& l, quint64& r){
    Q_ASSERT((size_t)l.size() >= sizeof(quint64));
    auto d = l.left(sizeof(quint64));
    memcpy(&r, d.data(), sizeof(quint64));
    l = l.mid(sizeof(quint64));
    return l;
}

QByteArray& operator>>(QByteArray& l, double& r){
    Q_ASSERT((size_t)l.size() >= sizeof(double));
    auto d = l.left(sizeof(double));
    memcpy(&r, d.data(), sizeof(double));
    l = l.mid(sizeof(double));
    return l;
}

QByteArray& operator<<(QByteArray& l, qint8 r){ return l.append(const_cast<const char*>(reinterpret_cast<char*>(&r)), sizeof(qint8)); }

QByteArray& operator<<(QByteArray& l, qint16 r){ return l.append(const_cast<const char*>(reinterpret_cast<char*>(&r)), sizeof(qint16)); }

QByteArray& operator<<(QByteArray& l, qint32 r){ return l.append(const_cast<const char*>(reinterpret_cast<char*>(&r)), sizeof(qint32)); }

QByteArray& operator<<(QByteArray& l, qint64 r){ return l.append(const_cast<const char*>(reinterpret_cast<char*>(&r)), sizeof(qint64)); }

QByteArray& operator<<(QByteArray& l, quint8 r){ return l.append(const_cast<const char*>(reinterpret_cast<char*>(&r)), sizeof(quint8));  }

QByteArray& operator<<(QByteArray& l, quint16 r){ return l.append(const_cast<const char*>(reinterpret_cast<char*>(&r)), sizeof(quint16)); }

QByteArray& operator<<(QByteArray& l, quint32 r){ return l.append(const_cast<const char*>(reinterpret_cast<char*>(&r)), sizeof(quint32)); }

QByteArray& operator<<(QByteArray& l, quint64 r){ return l.append(const_cast<const char*>(reinterpret_cast<char*>(&r)), sizeof(quint64)); }

QByteArray& operator<<(QByteArray& l, double r){ return l.append(const_cast<const char*>(reinterpret_cast<char*>(&r)), sizeof(double)); }
