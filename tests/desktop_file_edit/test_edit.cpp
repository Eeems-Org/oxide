#include "test_edit.h"

#include <QCommandLineParser>
#include <QJsonObject>

#include "../../applications/desktop-file-edit/edit.h"

// Helper: create a parser with all edit options registered and parse a
// QStringList of arguments. The first argument in args must be the app name.
static QCommandLineParser*
makeParser(const QStringList& args) {
  auto* parser = new QCommandLineParser();
  addEditOptions(*parser);
  // parse() does not require QCoreApplication and does not call exit on error
  parser->parse(args);
  return parser;
}

test_Edit::test_Edit() {}
test_Edit::~test_Edit() {}

// ---------------------------------------------------------------------------
// validateSetKeyValueOptions tests
// ---------------------------------------------------------------------------

void
test_Edit::test_validate_noSetKeyOrValue_returnsTrue() {
  // When neither --set-key nor --set-value is used the function always returns
  // true regardless of other options.
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(QStringList() << "app");
  QVERIFY(validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validate_validPair_returnsTrue() {
  // A single --set-key / --set-value pair in the correct order is valid.
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(
    QStringList() << "app" << "--set-key" << "myKey" << "--set-value"
                  << "myValue"
  );
  QVERIFY(validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validate_setValueBeforeSetKey_returnsFalse() {
  // --set-value appearing before any --set-key must be rejected.
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(QStringList() << "app" << "--set-value" << "v");
  QVERIFY(!validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validate_setKeyWithoutFollowingSetValue_returnsFalse() {
  // --set-key not immediately followed by --set-value must be rejected.
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(QStringList() << "app" << "--set-key" << "k");
  QVERIFY(!validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validate_multipleValidPairs_returnsTrue() {
  // Multiple consecutive valid pairs should all pass.
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(
    QStringList() << "app" << "--set-key" << "k1" << "--set-value" << "v1"
                  << "--set-key" << "k2" << "--set-value" << "v2"
  );
  QVERIFY(validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validate_setKeyWithOtherOptionBetween_returnsFalse() {
  // If a non-set-value option appears between --set-key and the expected
  // --set-value, validation must fail.
  QCommandLineParser parser;
  addEditOptions(parser);
  // --set-key k --set-icon icon -- here set-icon appears where set-value is
  // expected
  parser.parse(
    QStringList() << "app" << "--set-key" << "k" << "--set-icon" << "icon"
  );
  QVERIFY(!validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validate_onlySetKey_returnsFalse() {
  // --set-key provided with no following --set-value at all.
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(QStringList() << "app" << "--set-key" << "k");
  QVERIFY(!validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validate_onlySetValue_returnsFalse() {
  // --set-value with no preceding --set-key must be rejected.
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(QStringList() << "app" << "--set-value" << "v");
  QVERIFY(!validateSetKeyValueOptions(parser));
}

// ---------------------------------------------------------------------------
// applyChanges tests
// ---------------------------------------------------------------------------

void
test_Edit::test_apply_copyNameToGenericName_setsDisplayName() {
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(QStringList() << "app" << "--copy-name-to-generic-name");
  QJsonObject reg;
  applyChanges(parser, reg, "MyApp");
  QVERIFY(reg.contains("displayName"));
  QCOMPARE(reg["displayName"].toString(), QString("MyApp"));
}

void
test_Edit::test_apply_removeKey_removesExistingKey() {
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(QStringList() << "app" << "--remove-key" << "someKey");
  QJsonObject reg;
  reg["someKey"] = "someValue";
  applyChanges(parser, reg, "app");
  QVERIFY(!reg.contains("someKey"));
}

void
test_Edit::test_apply_removeKey_nonExistentKey_noError() {
  // Removing a key that doesn't exist should not add it or throw.
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(QStringList() << "app" << "--remove-key" << "ghost");
  QJsonObject reg;
  reg["other"] = "value";
  applyChanges(parser, reg, "app");
  QVERIFY(!reg.contains("ghost"));
  QVERIFY(reg.contains("other"));
}

void
test_Edit::test_apply_setGenericName_setsDisplayname() {
  // Note: the implementation stores value in reg["displayname"] (lowercase).
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(
    QStringList() << "app" << "--set-generic-name" << "My Generic Name"
  );
  QJsonObject reg;
  applyChanges(parser, reg, "app");
  QVERIFY(reg.contains("displayname"));
  QCOMPARE(reg["displayname"].toString(), QString("My Generic Name"));
}

void
test_Edit::test_apply_setIcon_setsIcon() {
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(QStringList() << "app" << "--set-icon" << "my-icon");
  QJsonObject reg;
  applyChanges(parser, reg, "app");
  QVERIFY(reg.contains("icon"));
  QCOMPARE(reg["icon"].toString(), QString("my-icon"));
}

void
test_Edit::test_apply_setKeyValue_setsArbitraryKey() {
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(
    QStringList() << "app" << "--set-key" << "customKey" << "--set-value"
                  << "customValue"
  );
  QJsonObject reg;
  applyChanges(parser, reg, "app");
  QVERIFY(reg.contains("customKey"));
  QCOMPARE(reg["customKey"].toString(), QString("customValue"));
}

void
test_Edit::test_apply_multipleSetKeyValues_setsAllKeys() {
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(
    QStringList() << "app" << "--set-key" << "key1" << "--set-value" << "val1"
                  << "--set-key" << "key2" << "--set-value" << "val2"
  );
  QJsonObject reg;
  applyChanges(parser, reg, "app");
  QVERIFY(reg.contains("key1"));
  QCOMPARE(reg["key1"].toString(), QString("val1"));
  QVERIFY(reg.contains("key2"));
  QCOMPARE(reg["key2"].toString(), QString("val2"));
}

void
test_Edit::test_apply_noOptions_doesNotModifyReg() {
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(QStringList() << "app");
  QJsonObject reg;
  reg["existing"] = "value";
  applyChanges(parser, reg, "app");
  QCOMPARE(reg.keys().size(), 1);
  QCOMPARE(reg["existing"].toString(), QString("value"));
}

void
test_Edit::test_apply_setIconOverridesExisting() {
  // A second --set-icon call should overwrite the previous icon value.
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(
    QStringList() << "app" << "--set-icon" << "first-icon" << "--set-icon"
                  << "second-icon"
  );
  QJsonObject reg;
  applyChanges(parser, reg, "app");
  // The last set-icon wins because each invocation overwrites reg["icon"]
  QVERIFY(reg.contains("icon"));
  QCOMPARE(reg["icon"].toString(), QString("second-icon"));
}

void
test_Edit::test_apply_copyNameToGenericName_usesProvidedName() {
  // The name argument to applyChanges is used as the displayName value.
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(QStringList() << "app" << "--copy-name-to-generic-name");
  QJsonObject reg;
  applyChanges(parser, reg, "SpecificName");
  QCOMPARE(reg["displayName"].toString(), QString("SpecificName"));
}

void
test_Edit::test_apply_setGenericName_overridesExisting() {
  // Calling --set-generic-name twice: the second invocation overwrites.
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(
    QStringList() << "app" << "--set-generic-name" << "First"
                  << "--set-generic-name" << "Second"
  );
  QJsonObject reg;
  applyChanges(parser, reg, "app");
  QCOMPARE(reg["displayname"].toString(), QString("Second"));
}

// ---------------------------------------------------------------------------
// addEditOptions tests
// ---------------------------------------------------------------------------

void
test_Edit::test_addEditOptions_registersAllOptions() {
  QCommandLineParser parser;
  addEditOptions(parser);
  // Verify the parser recognises all the expected option names.
  const QStringList knownOptions = {
    "set-key",
    "set-value",
    "set-name",
    "copy-name-to-generic-name",
    "set-generic-name",
    "copy-generic-name-to-name",
    "set-comment",
    "set-icon",
    "add-category",
    "remove-category",
    "add-mime-type",
    "remove-mime-type",
    "add-only-show-in",
    "remove-only-show-in",
    "add-not-show-in",
    "remove-not-show-in",
    "remove-key",
  };
  // parse with no args so the parser is initialised; use a dummy arg list
  parser.parse(QStringList() << "app");
  for (const QString& opt : knownOptions) {
    // QCommandLineParser::isSet() on an unset known option should not crash
    // and should return false; if the option is unknown it would assert/warn.
    // We simply verify the option was registered by attempting to set it.
    QCommandLineParser testParser;
    addEditOptions(testParser);
    testParser.parse(QStringList() << "app" << ("--" + opt) << "dummy");
    // If the option was not registered, parse would leave it as positional arg
    // or an unknown option. Check it is at least known to the parser.
    QVERIFY2(
      !testParser.unknownOptionNames().contains(opt),
      qPrintable("Option not registered: --" + opt)
    );
  }
}

DECLARE_TEST(test_Edit)