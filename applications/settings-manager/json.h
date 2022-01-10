#ifndef JSON_H
#define JSON_H
#include <QVariant>
#include <QDBusArgument>
#include <QDebug>
#include <QJsonValue>
#include <QJsonDocument>
#include <QJsonArray>

QVariant sanitizeForJson(QVariant value);
QVariant decodeDBusArgument(const QDBusArgument& arg){
    auto type = arg.currentType();
    if(type == QDBusArgument::BasicType || type == QDBusArgument::VariantType){
        return sanitizeForJson(arg.asVariant());
    }
    if(type == QDBusArgument::ArrayType){
        QVariantList list;
        arg.beginArray();
        while(!arg.atEnd()){
            list.append(decodeDBusArgument(arg));
        }
        arg.endArray();
        return sanitizeForJson(list);
    }
    if(type == QDBusArgument::MapType){
        qDebug() << "Map Type";
        QMap<QVariant, QVariant> map;
        arg.beginMap();
        while(!arg.atEnd()){
            arg.beginMapEntry();
            auto key = decodeDBusArgument(arg);
            auto value = decodeDBusArgument(arg);
            arg.endMapEntry();
            map.insert(sanitizeForJson(key), sanitizeForJson(value));
        }
        arg.endMap();
        return sanitizeForJson(QVariant::fromValue(map));
    }
    qDebug() << "Unable to sanitize QDBusArgument as it is an unknown type";
    return QVariant();
}
QVariant sanitizeForJson(QVariant value){
    auto userType = value.userType();
    if(userType == QMetaType::type("QDBusObjectPath")){
        return value.value<QDBusObjectPath>().path();
    }
    if(userType == QMetaType::type("QDBusSignature")){
        return value.value<QDBusSignature>().signature();
    }
    if(userType == QMetaType::type("QDBusVariant")){
        return value.value<QDBusVariant>().variant();
    }
    if(userType == QMetaType::type("QDBusArgument")){
        return decodeDBusArgument(value.value<QDBusArgument>());
    }
    if(userType == QMetaType::type("QList<QDBusVariant>")){
        QVariantList list;
        for(auto value : value.value<QList<QDBusVariant>>()){
            list.append(sanitizeForJson(value.variant()));
        }
        return list;
    }
    if(userType == QMetaType::type("QList<QDBusSignature>")){
        QStringList list;
        for(auto value : value.value<QList<QDBusSignature>>()){
            list.append(value.signature());
        }
        return list;
    }
    if(userType == QMetaType::type("QList<QDBusObjectPath>")){
        QStringList list;
        for(auto value : value.value<QList<QDBusObjectPath>>()){
            list.append(value.path());
        }
        return list;
    }
    if(userType == QMetaType::QByteArray){
        auto byteArray = value.toByteArray();
        QVariantList list;
        for(auto byte : byteArray){
            list.append(byte);
        }
        return list;
    }
    if(userType == QMetaType::QVariantMap){
        QVariantMap map;
        auto input = value.toMap();
        for(auto key : input.keys()){
            map.insert(key, sanitizeForJson(input[key]));
        }
        return map;
    }
    if(userType == QMetaType::QVariantList){
        QVariantList list = value.toList();
        QMutableListIterator<QVariant> i(list);
        while(i.hasNext()){
            i.setValue(sanitizeForJson(i.next()));
        }
        return list;
    }
    return value;
}
QString toJson(QVariant value){
    if(value.isNull()){
        return "null";
    }
    auto jsonVariant = QJsonValue::fromVariant(sanitizeForJson(value));
    if(jsonVariant.isNull()){
        return "null";
    }
    if(jsonVariant.isUndefined()){
        return "undefined";
    }
    QJsonArray jsonArray;
    jsonArray.append(jsonVariant);
    QJsonDocument doc(jsonArray);
    auto json = doc.toJson(QJsonDocument::Compact);
    return json.mid(1, json.length() - 2);
}
QVariant fromJson(QByteArray json){
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson("[" + json + "]", &error);
    if(error.error != QJsonParseError::NoError){
        qDebug() << "Unable to read json value" << error.errorString();
        qDebug() << "Value to parse" << json;
    }
    return doc.array().first().toVariant();
}

#endif // JSON_H
