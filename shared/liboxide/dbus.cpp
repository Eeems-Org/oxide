#include "dbus.h"

//QDBusArgument& operator<<(QDBusArgument& argument, const QRect& rect){
//    argument.beginStructure();
//    argument << rect.x() << rect.y() << rect.width() << rect.height();
//    argument.endStructure();
//    return argument;
//}

//const QDBusArgument& operator>>(const QDBusArgument& argument, QRect rect){
//    argument.beginStructure();
//    rect.setX(argument.asVariant().toInt());
//    rect.setY(argument.asVariant().toInt());
//    rect.setWidth(argument.asVariant().toInt());
//    rect.setHeight(argument.asVariant().toInt());
//    argument.endStructure();
//    return argument;
//}

//namespace Oxide{
//    void registerDBusTypes(){
//        qDBusRegisterMetaType<QRect>();
//    }
//}
