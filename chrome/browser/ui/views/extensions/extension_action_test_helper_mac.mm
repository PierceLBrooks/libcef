// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_action_test_helper.h"

#include <AppKit/AppKit.h>

#import "ui/base/test/windowed_nsnotification_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool ExtensionActionTestHelper::WaitForPopup() {
  NSWindow* window = GetPopupNativeView().GetNativeNSView().window;
  if (!window)
    return false;

  if (window.keyWindow) {
    return true;
  }

  WindowedNSNotificationObserver* waiter =
      [[WindowedNSNotificationObserver alloc]
          initForNotification:NSWindowDidBecomeKeyNotification
                       object:window];

  BOOL notification_observed = [waiter wait];
  return notification_observed && window.keyWindow;
}
