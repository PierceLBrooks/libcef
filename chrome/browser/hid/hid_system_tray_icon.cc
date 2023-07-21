// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_system_tray_icon.h"

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"

// static
gfx::ImageSkia HidSystemTrayIcon::GetStatusTrayIcon() {
  return gfx::CreateVectorIcon(vector_icons::kVideogameAssetIcon,
                               gfx::kGoogleGrey300);
}

// static
std::u16string HidSystemTrayIcon::GetTitleLabel(size_t num_origins,
                                                size_t num_connections) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (num_origins == 1) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_WEBHID_SYSTEM_TRAY_ICON_TITLE_SINGLE_EXTENSION,
        static_cast<int>(num_connections));
  }
  return l10n_util::GetPluralStringFUTF16(
      IDS_WEBHID_SYSTEM_TRAY_ICON_TITLE_MULTIPLE_EXTENSIONS,
      static_cast<int>(num_connections));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  NOTREACHED_NORETURN();
}

// static
std::u16string HidSystemTrayIcon::GetContentSettingsLabel() {
  return l10n_util::GetStringUTF16(IDS_WEBHID_SYSTEM_TRAY_ICON_HID_SETTINGS);
}

HidSystemTrayIcon::HidSystemTrayIcon() = default;
HidSystemTrayIcon::~HidSystemTrayIcon() = default;
