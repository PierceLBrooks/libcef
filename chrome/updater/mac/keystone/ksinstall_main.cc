// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/mac/keystone/ksinstall.h"

int main(int argc, char* argv[]) {
  return updater::KSInstallMain(argc, argv);
}
