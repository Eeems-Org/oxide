/*!
 * \addtogroup
 * \brief Operators to extend the Qt type system
 * @{
 * \file
 */
#pragma once
#include <QByteArray>

QByteArray& operator>>(QByteArray& l, qint8& r);

QByteArray& operator>>(QByteArray& l, qint16& r);

QByteArray& operator>>(QByteArray& l, qint32& r);

QByteArray& operator>>(QByteArray& l, qint64& r);

QByteArray& operator>>(QByteArray& l, quint8& r);

QByteArray& operator>>(QByteArray& l, quint16& r);

QByteArray& operator>>(QByteArray& l, quint32& r);

QByteArray& operator>>(QByteArray& l, quint64& r);

QByteArray& operator>>(QByteArray& l, double& r);

QByteArray& operator<<(QByteArray& l, qint8 r);

QByteArray& operator<<(QByteArray& l, qint16 r);

QByteArray& operator<<(QByteArray& l, qint32 r);

QByteArray& operator<<(QByteArray& l, qint64 r);

QByteArray& operator<<(QByteArray& l, quint8 r);

QByteArray& operator<<(QByteArray& l, quint16 r);

QByteArray& operator<<(QByteArray& l, quint32 r);

QByteArray& operator<<(QByteArray& l, quint64 r);

QByteArray& operator<<(QByteArray& l, double r);
