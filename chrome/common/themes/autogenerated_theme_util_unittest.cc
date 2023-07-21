// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/themes/autogenerated_theme_util.h"

#include <string>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/color_utils.h"

namespace {

TEST(AutogeneratedThemeUtil, GetContrastingColor) {
  constexpr float kChange = 0.2f;

  // White color for black background.
  EXPECT_EQ(SK_ColorWHITE, GetContrastingColor(SK_ColorBLACK, kChange));

  // Lighter color for too dark colors.
  constexpr SkColor kDarkBackground = SkColorSetARGB(255, 50, 0, 50);
  EXPECT_LT(color_utils::GetRelativeLuminance(kDarkBackground),
            color_utils::GetRelativeLuminance(
                GetContrastingColor(kDarkBackground, kChange)));

  // Darker color for light backgrounds.
  EXPECT_GT(color_utils::GetRelativeLuminance(SK_ColorWHITE),
            color_utils::GetRelativeLuminance(
                GetContrastingColor(SK_ColorWHITE, kChange)));

  constexpr SkColor kLightBackground = SkColorSetARGB(255, 100, 0, 100);
  EXPECT_GT(color_utils::GetRelativeLuminance(kLightBackground),
            color_utils::GetRelativeLuminance(
                GetContrastingColor(kLightBackground, kChange)));
}

}  // namespace
