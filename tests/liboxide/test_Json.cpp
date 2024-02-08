#include "test_Json.h"

#include <liboxide/json.h>
#include <QJsonObject>

test_Json::test_Json(){ }
test_Json::~test_Json(){ }

void test_Json::test_decodeDBusArgument(){
    // TODO - Oxide::JSON::decodeDBusArgument();
}

void test_Json::test_sanitizeForJson(){
    // TODO - Oxide::JSON::sanitizeForJson();
}

void test_Json::test_toJson(){
    QCOMPARE(Oxide::JSON::toJson(QVariant()), "null");
    QCOMPARE(Oxide::JSON::toJson(true), "true");
    QCOMPARE(Oxide::JSON::toJson(false), "false");
    QCOMPARE(Oxide::JSON::toJson(10), "10");
    QCOMPARE(Oxide::JSON::toJson(10.1), "10.1");
    QCOMPARE(Oxide::JSON::toJson("10"), "\"10\"");
    QJsonObject obj;
    obj.insert("value", 10);
    QCOMPARE(Oxide::JSON::toJson(obj), "{\"value\":10}");
    QCOMPARE(Oxide::JSON::toJson(obj, QJsonDocument::Indented), "{\n    \"value\": 10\n}\n");
    QJsonArray arr;
    arr.append(10);
    arr.append("10");
    QCOMPARE(Oxide::JSON::toJson(arr), "[10,\"10\"]");
    QCOMPARE(Oxide::JSON::toJson(arr, QJsonDocument::Indented), "[\n    10,\n    \"10\"\n]\n");
}

void test_Json::test_fromJson(){
    QVariant variant = Oxide::JSON::fromJson("null");
    QVERIFY(variant.isNull());
    variant = Oxide::JSON::fromJson("true");
    QVERIFY(variant.isValid());
    QVERIFY(!variant.isNull());
    QCOMPARE(variant.toBool(), true);
    variant = Oxide::JSON::fromJson("false");
    QVERIFY(variant.isValid());
    QVERIFY(!variant.isNull());
    QCOMPARE(variant.toBool(), false);
    variant = Oxide::JSON::fromJson("1");
    QVERIFY(variant.isValid());
    QVERIFY(!variant.isNull());
    QCOMPARE(variant.toInt(), 1);
    variant = Oxide::JSON::fromJson("\"1\"");
    QVERIFY(variant.isValid());
    QVERIFY(!variant.isNull());
    QCOMPARE(variant.toString(), "1");
    variant = Oxide::JSON::fromJson("{\"value\": 10}");
    QVERIFY(variant.isValid());
    QVERIFY(!variant.isNull());
    QJsonObject obj = variant.toJsonObject();
    QVERIFY(obj.keys().contains("value"));
    QCOMPARE(obj.value("value"), 10);
    variant = Oxide::JSON::fromJson("[10, \"10\"]");
    QVERIFY(variant.isValid());
    QVERIFY(!variant.isNull());
    QJsonArray arr = variant.toJsonArray();
    QCOMPARE(arr.count(), 2);
    QJsonValue value = arr.first();
    QVERIFY(value.isDouble());
    QCOMPARE(value.toDouble(), 10);
    value = arr.last();
    QVERIFY(value.isString());
    QCOMPARE(value.toString(), "10");
}

DECLARE_TEST(test_Json)
