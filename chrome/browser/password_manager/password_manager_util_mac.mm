// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_manager_util_mac.h"

#include <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#include <Security/Authorization.h>

#include "base/apple/bundle_locations.h"
#include "base/mac/authorization_util.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_authorizationref.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

NSString* UserAuthenticationRightName() {
  // The authentication right name is of the form
  // `org.chromium.Chromium.access-passwords` or
  // `com.google.Chrome.access-passwords`.
  return [[base::apple::MainBundle() bundleIdentifier]
      stringByAppendingString:@".access-passwords"];
}

bool EnsureAuthorizationRightExists() {
  NSString* rightName = UserAuthenticationRightName();
  // If the authorization right already exists there is nothing to do.
  if (AuthorizationRightGet(rightName.UTF8String, nullptr) ==
      errAuthorizationSuccess) {
    return true;
  }

  // The authorization right does not exist so create it.
  base::mac::ScopedAuthorizationRef authorization =
      base::mac::CreateAuthorization();
  if (!authorization) {
    return false;
  }

  // Create a right which requires that the user authenticate as the session
  // owner. The prompt must be specified each time the right is requested.
  OSStatus status =
      AuthorizationRightSet(authorization, rightName.UTF8String,
                            CFSTR(kAuthorizationRuleAuthenticateAsSessionUser),
                            nullptr, nullptr, nullptr);
  if (status != errAuthorizationSuccess) {
    OSSTATUS_LOG(ERROR, status) << "AuthorizationRightSet";
    return false;
  }

  return true;
}

}  // namespace

namespace password_manager_util_mac {

bool AuthenticateUser(std::u16string prompt_string) {
  if (!EnsureAuthorizationRightExists()) {
    return false;
  }

  NSString* rightName = UserAuthenticationRightName();
  AuthorizationItem right_items[] = {{rightName.UTF8String, 0, nullptr, 0}};
  AuthorizationRights rights = {std::size(right_items), right_items};

  base::ScopedCFTypeRef<CFStringRef> prompt =
      base::SysUTF16ToCFStringRef(prompt_string);

  // Pass kAuthorizationFlagDestroyRights to prevent the OS from saving the
  // authorization and not prompting the user when future requests are made.
  base::mac::ScopedAuthorizationRef authorization =
      base::mac::GetAuthorizationRightsWithPrompt(
          &rights, prompt, kAuthorizationFlagDestroyRights);
  return static_cast<bool>(authorization);
}

std::u16string GetMessageForLoginPrompt(
    password_manager::ReauthPurpose purpose) {
  switch (purpose) {
    case password_manager::ReauthPurpose::VIEW_PASSWORD:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
    case password_manager::ReauthPurpose::COPY_PASSWORD:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_COPY_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
    case password_manager::ReauthPurpose::EDIT_PASSWORD:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_EDIT_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
    case password_manager::ReauthPurpose::EXPORT:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_EXPORT_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
    case password_manager::ReauthPurpose::IMPORT:
      return l10n_util::GetStringUTF16(
          IDS_PASSWORDS_PAGE_IMPORT_AUTHENTICATION_PROMPT_BIOMETRIC_SUFFIX);
  }
}

}  // namespace password_manager_util_mac
