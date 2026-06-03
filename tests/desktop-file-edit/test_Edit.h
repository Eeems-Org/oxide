#pragma once

#include "autotest.h"

class test_Edit : public QObject {
  Q_OBJECT

public:
  test_Edit();
  ~test_Edit();

private slots:
  // addEditOptions tests
  void test_addEditOptions_addsAllOptions();

  // validateSetKeyValueOptions tests
  void test_validateSetKeyValueOptions_noKeyValueOptions_returnsTrue();
  void test_validateSetKeyValueOptions_setKeyThenSetValue_returnsTrue();
  void test_validateSetKeyValueOptions_setValueWithoutSetKey_returnsFalse();
  void test_validateSetKeyValueOptions_setKeyWithoutSetValue_returnsFalse();
  void test_validateSetKeyValueOptions_multipleKeyValuePairs_returnsTrue();
  void test_validateSetKeyValueOptions_setKeyAsLastOption_returnsFalse();

  // applyChanges tests
  void test_applyChanges_setKeyValue_updatesObject();
  void test_applyChanges_removeKey_removesFromObject();
  void test_applyChanges_copyNameToGenericName_setsDisplayName();
  void test_applyChanges_setGenericName_setsDisplayname();
  void test_applyChanges_setIcon_updatesIcon();
  void test_applyChanges_multipleSetKeyValue_updatesAll();
  void test_applyChanges_removeNonExistentKey_noError();
  void test_applyChanges_emptyParser_noChanges();
  void test_applyChanges_setIconMultipleTimes_usesLastValue();
};