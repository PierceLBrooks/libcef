// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/side_panel/companion/companion_utils.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/companion/core/constants.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace companion {

void RegisterPrefs(TestingPrefServiceSimple* pref_service) {
  pref_service->registry()->RegisterBooleanPref(
      prefs::kSidePanelCompanionEntryPinnedToToolbar, false);
  pref_service->registry()->RegisterBooleanPref(
      companion::kExpsOptInStatusGrantedPref, false);
}

TEST(CompanionUtilsTest, PinnedStateCommandlineOverridePinned) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceCompanionPinnedState, "pinned");

  UpdateCompanionDefaultPinnedToToolbarState(&pref_service);
  EXPECT_EQ(
      pref_service.GetBoolean(prefs::kSidePanelCompanionEntryPinnedToToolbar),
      true);
}

TEST(CompanionUtilsTest, PinnedStateCommandlineOverrideUnpinned) {
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceCompanionPinnedState, "unpinned");

  UpdateCompanionDefaultPinnedToToolbarState(&pref_service);
  EXPECT_EQ(
      pref_service.GetBoolean(prefs::kSidePanelCompanionEntryPinnedToToolbar),
      false);
}

TEST(CompanionUtilsTest, UpdatePinnedStateDefaultUnpinnedLabsOverride) {
  base::test::ScopedFeatureList scoped_feature_list;
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);

  scoped_feature_list.InitAndDisableFeature(
      ::features::kSidePanelCompanionDefaultPinned);
  pref_service.SetBoolean(companion::kExpsOptInStatusGrantedPref, true);

  UpdateCompanionDefaultPinnedToToolbarState(&pref_service);
  EXPECT_EQ(
      pref_service.GetBoolean(prefs::kSidePanelCompanionEntryPinnedToToolbar),
      true);
}

TEST(CompanionUtilsTest, UpdatePinnedStateDefaultPinned) {
  base::test::ScopedFeatureList scoped_feature_list;
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);

  scoped_feature_list.InitAndEnableFeature(
      ::features::kSidePanelCompanionDefaultPinned);
  pref_service.SetBoolean(companion::kExpsOptInStatusGrantedPref, false);

  UpdateCompanionDefaultPinnedToToolbarState(&pref_service);
  EXPECT_EQ(
      pref_service.GetBoolean(prefs::kSidePanelCompanionEntryPinnedToToolbar),
      true);
}

TEST(CompanionUtilsTest, UpdatePinnedStateDefaultUnPinned) {
  base::test::ScopedFeatureList scoped_feature_list;
  TestingPrefServiceSimple pref_service;
  RegisterPrefs(&pref_service);

  scoped_feature_list.InitAndDisableFeature(
      ::features::kSidePanelCompanionDefaultPinned);
  pref_service.SetBoolean(companion::kExpsOptInStatusGrantedPref, false);

  UpdateCompanionDefaultPinnedToToolbarState(&pref_service);
  EXPECT_EQ(
      pref_service.GetBoolean(prefs::kSidePanelCompanionEntryPinnedToToolbar),
      false);
}

TEST(CompanionUtilsTest, PromoNotShownOnEmptyURL) {
  GURL empty_url("");
  base::test::ScopedFeatureList scoped_feature_list;
  TestingPrefServiceSimple pref_service;

  // Enable CSC and pinned state
  scoped_feature_list.InitAndEnableFeature(
      features::internal::kSidePanelCompanion);
  pref_service.registry()->RegisterBooleanPref(
      prefs::kSidePanelCompanionEntryPinnedToToolbar, true);

  EXPECT_FALSE(ShouldTriggerCompanionFeaturePromo(empty_url, &pref_service));
}

TEST(CompanionUtilsTest, PromoNotShownOnNewTabPage) {
  GURL ntp_url("chrome://newtab");
  base::test::ScopedFeatureList scoped_feature_list;
  TestingPrefServiceSimple pref_service;

  // Enable CSC and pinned state
  scoped_feature_list.InitAndEnableFeature(
      features::internal::kSidePanelCompanion);
  pref_service.registry()->RegisterBooleanPref(
      prefs::kSidePanelCompanionEntryPinnedToToolbar, true);

  EXPECT_FALSE(ShouldTriggerCompanionFeaturePromo(ntp_url, &pref_service));
}

TEST(CompanionUtilsTest, PromoNotShownOnChromePage) {
  GURL csc_flag_url("chrome://flags#csc");
  base::test::ScopedFeatureList scoped_feature_list;
  TestingPrefServiceSimple pref_service;

  // Enable CSC and pinned state
  scoped_feature_list.InitAndEnableFeature(
      features::internal::kSidePanelCompanion);
  pref_service.registry()->RegisterBooleanPref(
      prefs::kSidePanelCompanionEntryPinnedToToolbar, true);

  EXPECT_FALSE(ShouldTriggerCompanionFeaturePromo(csc_flag_url, &pref_service));
}

TEST(CompanionUtilsTest, PromoNotShownOnNullptrPrefs) {
  GURL csc_flag_url("");

  EXPECT_FALSE(ShouldTriggerCompanionFeaturePromo(csc_flag_url, nullptr));
}

TEST(CompanionUtilsTest, PromoNotShownIfCscDisabled) {
  GURL valid_url("https://www.google.com");
  base::test::ScopedFeatureList scoped_feature_list;
  TestingPrefServiceSimple pref_service;

  // Enable CSC and pinned state
  scoped_feature_list.InitAndDisableFeature(
      features::internal::kSidePanelCompanion);
  pref_service.registry()->RegisterBooleanPref(
      prefs::kSidePanelCompanionEntryPinnedToToolbar, true);

  EXPECT_FALSE(ShouldTriggerCompanionFeaturePromo(valid_url, &pref_service));
}

TEST(CompanionUtilsTest, PromoNotShownIfCscNotPinned) {
  GURL valid_url("https://www.google.com");
  base::test::ScopedFeatureList scoped_feature_list;
  TestingPrefServiceSimple pref_service;

  // Enable CSC and pinned state
  scoped_feature_list.InitAndEnableFeature(
      features::internal::kSidePanelCompanion);
  pref_service.registry()->RegisterBooleanPref(
      prefs::kSidePanelCompanionEntryPinnedToToolbar, false);

  EXPECT_FALSE(ShouldTriggerCompanionFeaturePromo(valid_url, &pref_service));
}

TEST(CompanionUtilsTest, PromoShownOnValidUrlWithCscEnabledAndPinned) {
  GURL valid_url("https://www.google.com");
  base::test::ScopedFeatureList scoped_feature_list;
  TestingPrefServiceSimple pref_service;

  // Enable CSC and pinned state
  scoped_feature_list.InitAndEnableFeature(
      features::internal::kSidePanelCompanion);
  pref_service.registry()->RegisterBooleanPref(
      prefs::kSidePanelCompanionEntryPinnedToToolbar, true);

  EXPECT_TRUE(ShouldTriggerCompanionFeaturePromo(valid_url, &pref_service));
}

}  // namespace companion
