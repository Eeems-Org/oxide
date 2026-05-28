#include "dbus_types.h"

QDBusArgument&
operator<<(QDBusArgument& arg, const FrameBufferInfo& t)
{
    arg.beginStructure();
    arg << std::get<0>(t) << std::get<1>(t) << std::get<2>(t);
    arg.endStructure();
    return arg;
}

const QDBusArgument&
operator>>(const QDBusArgument& arg, FrameBufferInfo& t)
{
    int a, b, c;
    arg.beginStructure();
    arg >> a >> b >> c;
    arg.endStructure();
    t = { a, b, c };
    return arg;
}
