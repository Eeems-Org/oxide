#include "json.h"
#include "debug.h"

#include <QJsonValue>

static bool qIsNumericType(uint tp){
    static const qulonglong numericTypeBits =
        Q_UINT64_C(1) << QMetaType::Bool |
        Q_UINT64_C(1) << QMetaType::Double |
        Q_UINT64_C(1) << QMetaType::Float |
        Q_UINT64_C(1) << QMetaType::Char |
        Q_UINT64_C(1) << QMetaType::SChar |
        Q_UINT64_C(1) << QMetaType::UChar |
        Q_UINT64_C(1) << QMetaType::Short |
        Q_UINT64_C(1) << QMetaType::UShort |
        Q_UINT64_C(1) << QMetaType::Int |
        Q_UINT64_C(1) << QMetaType::UInt |
        Q_UINT64_C(1) << QMetaType::Long |
        Q_UINT64_C(1) << QMetaType::ULong |
        Q_UINT64_C(1) << QMetaType::LongLong |
        Q_UINT64_C(1) << QMetaType::ULongLong;
    return tp < (CHAR_BIT * sizeof numericTypeBits) ? numericTypeBits & (Q_UINT64_C(1) << tp) : false;
}

int compareAsString(const QVariant* v1, const QVariant* v2){
    int r = v1->toString().compare(v2->toString(), Qt::CaseInsensitive);
    if (r == 0) {
        return (v1->typeId() < v2->typeId()) ? -1 : 1;
    }
    return r;
}
int compare(const QVariant* v1, const QVariant* v2){
    if (qIsNumericType(v1->typeId()) && qIsNumericType(v2->typeId())){
        if(v1 == v2){
            return 0;
        }
        if(v1 < v2){
            return -1;
        }
        return 1;
    }
    if ((int)v1->typeId() >= (int)QMetaType::User) {
        const void* v1d = v1->constData();
        const void* v2d = v2->constData();
        auto ordering = v1->metaType().compare(v1d, v2d);
        // Can't convert to int for some reason
        if(ordering < 0){
            return -1;
        }
        if(ordering > 0){
            return 1;
        }
        return 0;
    }
    switch (v1->typeId()){
        case QMetaType::QDate:
            return v1->toDate() < v2->toDate() ? -1 : 1;
        case QMetaType::QTime:
            return v1->toTime() < v2->toTime() ? -1 : 1;
        case QMetaType::QDateTime:
            return v1->toDateTime() < v2->toDateTime() ? -1 : 1;
        case QMetaType::QStringList:
            return v1->toStringList() < v2->toStringList() ? -1 : 1;
        default:
            return compareAsString(v1, v2);
    }
}

bool operator<(const QVariant& lhs, const QVariant& rhs){
    const QVariant* v1 = &lhs;
    const QVariant* v2 = &rhs;
    if(lhs.typeId() != rhs.typeId()){
        if (v2->canConvert(v1->metaType())) {
            QVariant converted2 = *v2;
            if (converted2.convert(v1->metaType())){
                v2 = &converted2;
            }
        }
        if (v1->typeId() != v2->typeId() && v1->canConvert(v2->metaType())) {
            QVariant converted1 = *v1;
            if (converted1.convert(v2->metaType())){
                v1 = &converted1;
            }
        }
        if (v1->typeId() != v2->typeId()) {
            return compareAsString(v1, v2) < 0;
        }
    }
    return compare(v1, v2) < 0;
}

namespace Oxide::JSON {
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
        O_WARNING("Unable to decode QDBusArgument as it is an unknown type");
        return QVariant();
    }
    QVariant sanitizeForJson(QVariant value){
        auto userType = value.userType();
        if(userType == QMetaType::fromName("QDBusObjectPath").id()){
            return value.value<QDBusObjectPath>().path();
        }
        if(userType == QMetaType::fromName("QDBusSignature").id()){
            return value.value<QDBusSignature>().signature();
        }
        if(userType == QMetaType::fromName("QDBusVariant").id()){
            return value.value<QDBusVariant>().variant();
        }
        if(userType == QMetaType::fromName("QDBusArgument").id()){
            return decodeDBusArgument(value.value<QDBusArgument>());
        }
        if(userType == QMetaType::fromName("QList<QDBusVariant>").id()){
            QVariantList list;
            for(auto value : value.value<QList<QDBusVariant>>()){
                list.append(sanitizeForJson(value.variant()));
            }
            return list;
        }
        if(userType == QMetaType::fromName("QList<QDBusSignature>").id()){
            QStringList list;
            for(auto value : value.value<QList<QDBusSignature>>()){
                list.append(value.signature());
            }
            return list;
        }
        if(userType == QMetaType::fromName("QList<QDBusObjectPath>").id()){
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
    QString toJson(QVariant value, QJsonDocument::JsonFormat format){
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
        if(jsonVariant.isArray()){
            QJsonDocument doc(jsonVariant.toArray());
            return doc.toJson(format);
        }
        if(jsonVariant.isObject()){
            QJsonDocument doc(jsonVariant.toObject());
            return doc.toJson(format);
        }
        if(jsonVariant.isBool()){
            return jsonVariant.toBool() ? "true" : "false";
        }
        // Number, string or other unknown type
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
            O_WARNING("Unable to read json value" << error.errorString());
            O_WARNING("Value to parse" << json);
        }
        return doc.array().first().toVariant();
    }
    QVariant fromJson(QFile* file){ return fromJson(file->readAll()); }
}
