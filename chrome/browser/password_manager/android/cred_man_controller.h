// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CRED_MAN_CONTROLLER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CRED_MAN_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"

namespace webauthn {
class WebAuthnCredManDelegate;
}  // namespace webauthn

namespace password_manager {

class PasswordCredentialFiller;

// This class is responsible for the logic to show Credential Manager UI. The
// interaction with Credential Manager UI is delegated to WebAuthnCredMan class.
// Its lifecycle is tied to ChromePasswordManagerClient. CredManController is
// used in Android U+ only.
class CredManController : public base::SupportsWeakPtr<CredManController> {
 public:
  CredManController();

  CredManController(const CredManController&) = delete;
  CredManController& operator=(const CredManController&) = delete;

  ~CredManController();

  // Determines if the Android Credential Manager UI should be shown and shows
  // if required. Returns true if the Android Credential Manager UI is shown,
  // false otherwise.
  bool Show(raw_ptr<webauthn::WebAuthnCredManDelegate> cred_man_delegate,
            std::unique_ptr<PasswordCredentialFiller> filler,
            bool is_webauthn_form);

 private:
  std::unique_ptr<PasswordCredentialFiller> filler_;
};

}  // namespace password_manager

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_CRED_MAN_CONTROLLER_H_
