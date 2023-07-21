// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_FEATURES_H_
#define CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_FEATURES_H_

#include "base/feature_list.h"

namespace enterprise_idle {

// Controls whether the IdleTimeout policy is enabled.
BASE_DECLARE_FEATURE(kIdleTimeout);

}  // namespace enterprise_idle

#endif  // CHROME_BROWSER_ENTERPRISE_IDLE_IDLE_FEATURES_H_
