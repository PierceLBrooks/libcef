// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/gaia_credential.h"

#include "base/command_line.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_base.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/gcpw_strings.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/mdm_utils.h"
#include "content/public/common/content_switches.h"

namespace credential_provider {
CGaiaCredential::CGaiaCredential() = default;

CGaiaCredential::~CGaiaCredential() = default;

HRESULT CGaiaCredential::FinalConstruct() {
  LOGFN(VERBOSE);
  return S_OK;
}

void CGaiaCredential::FinalRelease() {
  LOGFN(VERBOSE);
}

HRESULT CGaiaCredential::GetUserGlsCommandline(
    base::CommandLine* command_line) {
  // In default add user flow, the user has to accept tos
  // every time. So we need to set the show_tos switch to 1.
  bool show_tos = IsGemEnabled();
  HRESULT hr = SetGaiaEndpointCommandLineIfNeeded(
      L"ep_setup_url", kGaiaSetupPath, IsGemEnabled(), show_tos, command_line);
  if (FAILED(hr)) {
    LOGFN(ERROR) << "Setting gaia url for gaia credential failed";
    return E_FAIL;
  }
  return S_OK;
}

}  // namespace credential_provider
