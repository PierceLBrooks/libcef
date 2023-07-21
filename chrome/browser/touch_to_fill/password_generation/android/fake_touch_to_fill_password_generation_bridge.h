// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_FAKE_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_FAKE_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_

#include <jni.h>
#include "chrome/browser/touch_to_fill/password_generation/android/touch_to_fill_password_generation_bridge.h"
#include "content/public/browser/web_contents.h"

class FakeTouchToFillPasswordGenerationBridge
    : public TouchToFillPasswordGenerationBridge {
 public:
  FakeTouchToFillPasswordGenerationBridge();
  ~FakeTouchToFillPasswordGenerationBridge() override;

  bool Show(content::WebContents* web_contents,
            base::WeakPtr<TouchToFillPasswordGenerationDelegate> delegate,
            std::u16string password,
            std::string account) override;
  void Hide() override;
  void OnDismissed(JNIEnv* env) override;

 private:
  base::WeakPtr<TouchToFillPasswordGenerationDelegate> delegate_;
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_GENERATION_ANDROID_FAKE_TOUCH_TO_FILL_PASSWORD_GENERATION_BRIDGE_H_
