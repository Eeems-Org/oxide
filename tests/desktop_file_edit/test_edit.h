#pragma once
#include "autotest.h"

class test_Edit : public QObject {
  Q_OBJECT

public:
  test_Edit();
  ~test_Edit();

private slots:
  // Tests for validateSetKeyValueOptions()
  void test_validate_noSetKeyOrValue_returnsTrue();
  void test_validate_validPair_returnsTrue();
  void test_validate_setValueBeforeSetKey_returnsFalse();
  void test_validate_setKeyWithoutFollowingSetValue_returnsFalse();
  void test_validate_multipleValidPairs_returnsTrue();
  void test_validate_setKeyWithOtherOptionBetween_returnsFalse();
  void test_validate_onlySetKey_returnsFalse();
  void test_validate_onlySetValue_returnsFalse();

  // Tests for applyChanges()
  void test_apply_copyNameToGenericName_setsDisplayName();
  void test_apply_removeKey_removesExistingKey();
  void test_apply_removeKey_nonExistentKey_noError();
  void test_apply_setGenericName_setsDisplayname();
  void test_apply_setIcon_setsIcon();
  void test_apply_setKeyValue_setsArbitraryKey();
  void test_apply_multipleSetKeyValues_setsAllKeys();
  void test_apply_noOptions_doesNotModifyReg();
  void test_apply_setIconOverridesExisting();
  void test_apply_copyNameToGenericName_usesProvidedName();
  void test_apply_setGenericName_overridesExisting();

  // Tests for addEditOptions()
  void test_addEditOptions_registersAllOptions();
};