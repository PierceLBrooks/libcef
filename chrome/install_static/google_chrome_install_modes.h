// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Brand-specific types and constants for Google Chrome.

#ifndef CHROME_INSTALL_STATIC_GOOGLE_CHROME_INSTALL_MODES_H_
#define CHROME_INSTALL_STATIC_GOOGLE_CHROME_INSTALL_MODES_H_

namespace install_static {

// Note: This list of indices must be kept in sync with the brand-specific
// resource strings in chrome/installer/util/prebuild/create_string_rc.
enum InstallConstantIndex {
  STABLE_INDEX,
  BETA_INDEX,
  DEV_INDEX,
  CANARY_INDEX,
  NUM_INSTALL_MODES,
};

}  // namespace install_static

#endif  // CHROME_INSTALL_STATIC_GOOGLE_CHROME_INSTALL_MODES_H_
