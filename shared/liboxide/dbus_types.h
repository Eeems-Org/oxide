#pragma once
#ifndef DBUS_TYPES_H
#define DBUS_TYPES_H
#include <QDBusArgument>
#include <QMetaType>

#include <tuple>

using FrameBufferInfo = std::tuple<int, int, int>;
Q_DECLARE_METATYPE(FrameBufferInfo);

QDBusArgument&
operator<<(QDBusArgument& arg, const FrameBufferInfo& t);

const QDBusArgument&
operator>>(const QDBusArgument& arg, FrameBufferInfo& t);
#endif
