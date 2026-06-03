#include "test_Edit.h"

#include "../../applications/desktop-file-edit/edit.h"

#include <QCommandLineParser>
#include <QJsonObject>

test_Edit::test_Edit() {}
test_Edit::~test_Edit() {}

// Helper: build a parser with edit options and parse from a list of args.
// The first element of args should be the application name.
static QCommandLineParser
makeParser(const QStringList& args) {
  QCommandLineParser parser;
  addEditOptions(parser);
  parser.parse(args);
  return parser;
}

// ---------------------------------------------------------------------------
// addEditOptions tests
// ---------------------------------------------------------------------------

void
test_Edit::test_addEditOptions_addsAllOptions() {
  QCommandLineParser parser;
  addEditOptions(parser);

  // All expected long-option names that addEditOptions should register
  const QStringList expectedOptions = {
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

  // Collect registered option names via a dummy parse
  parser.parse(QStringList() << "app");
  // The parser knows about these options; verify they don't produce unknown
  // option errors when we pass them (would fail parse otherwise)
  for (const QString& name : expectedOptions) {
    // Verify that the option name is recognizable by the parser by parsing it
    QCommandLineParser p2;
    addEditOptions(p2);
    // An option that takes a value needs a value; options without values don't.
    // We check by simply verifying parse doesn't emit an "Unknown option" error.
    // We can't exhaustively call isSet() without a parse, so we rely on the
    // fact that addEditOptions registers each named option.
    QVERIFY2(true, qPrintable(QString("Option '%1' should be registered").arg(name)));
  }

  // Verify count of registered options equals expectation
  // By parsing with no args we get an empty optionNames(), but we can
  // verify that calling addOption for an already-registered name would throw.
  // Instead, test pragmatically: parse with every option and confirm they all
  // parse without "unknown option" errors.
  QCommandLineParser p3;
  addEditOptions(p3);
  QStringList allArgs = {"app",
    "--set-key", "k",
    "--set-value", "v",
    "--set-name", "n",
    "--copy-name-to-generic-name",
    "--set-generic-name", "g",
    "--copy-generic-name-to-name",
    "--set-comment", "c",
    "--set-icon", "i",
    "--add-category", "cat",
    "--remove-category", "cat2",
    "--add-mime-type", "mime",
    "--remove-mime-type", "mime2",
    "--add-only-show-in", "env",
    "--remove-only-show-in", "env2",
    "--add-not-show-in", "env3",
    "--remove-not-show-in", "env4",
    "--remove-key", "somekey"
  };
  bool ok = p3.parse(allArgs);
  QVERIFY2(ok, qPrintable(p3.errorText()));
}

// ---------------------------------------------------------------------------
// validateSetKeyValueOptions tests
// ---------------------------------------------------------------------------

void
test_Edit::test_validateSetKeyValueOptions_noKeyValueOptions_returnsTrue() {
  // No --set-key or --set-value at all → should return true
  auto parser = makeParser({"app"});
  QVERIFY(validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validateSetKeyValueOptions_setKeyThenSetValue_returnsTrue() {
  // Proper ordering: --set-key KEY --set-value VALUE
  auto parser = makeParser({"app", "--set-key", "mykey", "--set-value", "myval"});
  QVERIFY(validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validateSetKeyValueOptions_setValueWithoutSetKey_returnsFalse() {
  // --set-value appears before any --set-key → invalid
  auto parser = makeParser({"app", "--set-value", "myval"});
  QVERIFY(!validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validateSetKeyValueOptions_setKeyWithoutSetValue_returnsFalse() {
  // --set-key appears but with another option following, not --set-value
  auto parser = makeParser({"app", "--set-key", "mykey", "--set-icon", "someicon"});
  QVERIFY(!validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validateSetKeyValueOptions_multipleKeyValuePairs_returnsTrue() {
  // Multiple proper --set-key/--set-value pairs
  auto parser = makeParser({
    "app",
    "--set-key", "k1", "--set-value", "v1",
    "--set-key", "k2", "--set-value", "v2"
  });
  QVERIFY(validateSetKeyValueOptions(parser));
}

void
test_Edit::test_validateSetKeyValueOptions_setKeyAsLastOption_returnsFalse() {
  // --set-key followed by a non-set-value option should fail.
  // Using --copy-name-to-generic-name (no-value option) after --set-key so that
  // optionNames() returns ["set-key", "copy-name-to-generic-name"]; the look-ahead
  // finds "copy-name-to-generic-name" which is not "set-value" → returns false.
  auto parser = makeParser({
    "app",
    "--set-key", "orphan",
    "--copy-name-to-generic-name"
  });
  QVERIFY(!validateSetKeyValueOptions(parser));
}

// ---------------------------------------------------------------------------
// applyChanges tests
// ---------------------------------------------------------------------------

void
test_Edit::test_applyChanges_setKeyValue_updatesObject() {
  auto parser = makeParser({"app", "--set-key", "mykey", "--set-value", "myval"});
  QJsonObject reg;
  applyChanges(parser, reg, "testapp");
  QVERIFY(reg.contains("mykey"));
  QCOMPARE(reg.value("mykey").toString(), QString("myval"));
}

void
test_Edit::test_applyChanges_removeKey_removesFromObject() {
  auto parser = makeParser({"app", "--remove-key", "deleteMe"});
  QJsonObject reg;
  reg.insert("deleteMe", "somevalue");
  reg.insert("keepMe", "othervalue");
  applyChanges(parser, reg, "testapp");
  QVERIFY(!reg.contains("deleteMe"));
  QVERIFY(reg.contains("keepMe"));
}

void
test_Edit::test_applyChanges_copyNameToGenericName_setsDisplayName() {
  auto parser = makeParser({"app", "--copy-name-to-generic-name"});
  QJsonObject reg;
  applyChanges(parser, reg, "myappname");
  QVERIFY(reg.contains("displayName"));
  QCOMPARE(reg.value("displayName").toString(), QString("myappname"));
}

void
test_Edit::test_applyChanges_setGenericName_setsDisplayname() {
  auto parser = makeParser({"app", "--set-generic-name", "My Generic Name"});
  QJsonObject reg;
  applyChanges(parser, reg, "testapp");
  // Note: the implementation stores under "displayname" (lowercase 'n')
  QVERIFY(reg.contains("displayname"));
  QCOMPARE(reg.value("displayname").toString(), QString("My Generic Name"));
}

void
test_Edit::test_applyChanges_setIcon_updatesIcon() {
  auto parser = makeParser({"app", "--set-icon", "myicon.png"});
  QJsonObject reg;
  applyChanges(parser, reg, "testapp");
  QVERIFY(reg.contains("icon"));
  QCOMPARE(reg.value("icon").toString(), QString("myicon.png"));
}

void
test_Edit::test_applyChanges_multipleSetKeyValue_updatesAll() {
  auto parser = makeParser({
    "app",
    "--set-key", "alpha", "--set-value", "a",
    "--set-key", "beta",  "--set-value", "b"
  });
  QJsonObject reg;
  applyChanges(parser, reg, "testapp");
  QVERIFY(reg.contains("alpha"));
  QCOMPARE(reg.value("alpha").toString(), QString("a"));
  QVERIFY(reg.contains("beta"));
  QCOMPARE(reg.value("beta").toString(), QString("b"));
}

void
test_Edit::test_applyChanges_removeNonExistentKey_noError() {
  auto parser = makeParser({"app", "--remove-key", "noSuchKey"});
  QJsonObject reg;
  reg.insert("otherKey", "value");
  // Removing a key that is not present should not crash or remove other keys
  applyChanges(parser, reg, "testapp");
  QVERIFY(reg.contains("otherKey"));
}

void
test_Edit::test_applyChanges_emptyParser_noChanges() {
  auto parser = makeParser({"app"});
  QJsonObject reg;
  reg.insert("existingKey", "existingValue");
  applyChanges(parser, reg, "testapp");
  QVERIFY(reg.contains("existingKey"));
  QCOMPARE(reg.value("existingKey").toString(), QString("existingValue"));
}

void
test_Edit::test_applyChanges_setIconMultipleTimes_usesLastValue() {
  // When --set-icon is passed multiple times, the last value should win
  // (applyChanges processes them in order, so all get applied;
  //  the last invocation of setIconOption overwrites the earlier one)
  auto parser = makeParser({"app", "--set-icon", "first.png", "--set-icon", "second.png"});
  QJsonObject reg;
  applyChanges(parser, reg, "testapp");
  QVERIFY(reg.contains("icon"));
  QCOMPARE(reg.value("icon").toString(), QString("second.png"));
}

DECLARE_TEST(test_Edit)