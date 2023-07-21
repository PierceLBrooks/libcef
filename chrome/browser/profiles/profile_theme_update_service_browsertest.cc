// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_theme_update_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin/profile_colors_util.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

class ProfileThemeUpdateServiceBrowserTest : public InProcessBrowserTest {
 public:
  ProfileAttributesEntry* GetProfileAttributesEntry() {
    CHECK(browser());
    CHECK(browser()->profile());
    ProfileAttributesEntry* entry =
        g_browser_process->profile_manager()
            ->GetProfileAttributesStorage()
            .GetProfileAttributesWithPath(browser()->profile()->GetPath());
    CHECK(entry);
    return entry;
  }

  ThemeService* theme_service() {
    return ThemeServiceFactory::GetForProfile(browser()->profile());
  }
};

// Tests that the profile theme colors are updated when an autogenerated theme
// is set up.
IN_PROC_BROWSER_TEST_F(ProfileThemeUpdateServiceBrowserTest,
                       PRE_AutogeneratedTheme) {
  EXPECT_EQ(GetProfileAttributesEntry()->GetProfileThemeColors(),
            GetDefaultProfileThemeColors());

  theme_service()->BuildAutogeneratedThemeFromColor(SK_ColorGREEN);
  ProfileThemeColors theme_colors =
      GetProfileAttributesEntry()->GetProfileThemeColors();
  EXPECT_NE(theme_colors, GetDefaultProfileThemeColors());

  // Check that a switch to another autogenerated theme updates the colors.
  theme_service()->BuildAutogeneratedThemeFromColor(SK_ColorMAGENTA);
  ProfileThemeColors theme_colors2 =
      GetProfileAttributesEntry()->GetProfileThemeColors();
  EXPECT_NE(theme_colors, theme_colors2);
  EXPECT_NE(theme_colors, GetDefaultProfileThemeColors());

  // Reset the cached colors to test that they're recreated on the next startup.
  GetProfileAttributesEntry()->SetProfileThemeColors(absl::nullopt);
  EXPECT_EQ(GetProfileAttributesEntry()->GetProfileThemeColors(),
            GetDefaultProfileThemeColors());
}

// Tests that the profile theme colors are updated on startup.
IN_PROC_BROWSER_TEST_F(ProfileThemeUpdateServiceBrowserTest,
                       AutogeneratedTheme) {
  EXPECT_NE(GetProfileAttributesEntry()->GetProfileThemeColors(),
            GetDefaultProfileThemeColors());
}

// Tests that switching to the default theme resets the colors.
IN_PROC_BROWSER_TEST_F(ProfileThemeUpdateServiceBrowserTest, DefaultTheme) {
  theme_service()->BuildAutogeneratedThemeFromColor(SK_ColorGREEN);
  EXPECT_NE(GetProfileAttributesEntry()->GetProfileThemeColors(),
            GetDefaultProfileThemeColors());

  theme_service()->UseDefaultTheme();
  EXPECT_EQ(GetProfileAttributesEntry()->GetProfileThemeColors(),
            GetDefaultProfileThemeColors());
}
