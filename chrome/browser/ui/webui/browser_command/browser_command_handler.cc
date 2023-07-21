// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/browser_command/browser_command_handler.h"

#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/command_updater_impl.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/new_tab_page/promos/promo_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/user_education/user_education_service.h"
#include "chrome/browser/ui/user_education/user_education_service_factory.h"
#include "chrome/browser/ui/views/user_education/browser_user_education_service.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/webui_url_constants.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/performance_manager/public/features.h"
#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"
#include "components/safe_browsing/core/common/safe_browsing_policy_handler.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/search/ntp_features.h"
#include "components/user_education/common/tutorial_identifier.h"
#include "components/user_education/common/tutorial_service.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/base/window_open_disposition_utils.h"

using browser_command::mojom::ClickInfoPtr;
using browser_command::mojom::Command;
using browser_command::mojom::CommandHandler;

// static
const char BrowserCommandHandler::kPromoBrowserCommandHistogramName[] =
    "NewTabPage.Promos.PromoBrowserCommand";

BrowserCommandHandler::BrowserCommandHandler(
    mojo::PendingReceiver<CommandHandler> pending_page_handler,
    Profile* profile,
    std::vector<browser_command::mojom::Command> supported_commands)
    : profile_(profile),
      supported_commands_(supported_commands),
      command_updater_(std::make_unique<CommandUpdaterImpl>(this)),
      page_handler_(this, std::move(pending_page_handler)) {
  if (supported_commands_.empty())
    return;

  EnableSupportedCommands();
}

BrowserCommandHandler::~BrowserCommandHandler() = default;

void BrowserCommandHandler::CanExecuteCommand(
    browser_command::mojom::Command command_id,
    CanExecuteCommandCallback callback) {
  if (!base::Contains(supported_commands_, command_id)) {
    std::move(callback).Run(false);
    return;
  }

  bool can_execute = false;
  switch (static_cast<Command>(command_id)) {
    case Command::kUnknownCommand:
      // Nothing to do.
      break;
    case Command::kOpenSafetyCheck:
      can_execute = !chrome::enterprise_util::IsBrowserManaged(profile_);
      break;
    case Command::kOpenSafeBrowsingEnhancedProtectionSettings: {
      bool managed = safe_browsing::SafeBrowsingPolicyHandler::
          IsSafeBrowsingProtectionLevelSetByPolicy(profile_->GetPrefs());
      bool already_enabled =
          safe_browsing::IsEnhancedProtectionEnabled(*(profile_->GetPrefs()));
      can_execute = !managed && !already_enabled;
    } break;
    case Command::kOpenFeedbackForm:
      can_execute = true;
      break;
    case Command::kOpenPrivacyGuide:
      can_execute = !chrome::enterprise_util::IsBrowserManaged(profile_) &&
                    !profile_->IsChild();
      base::UmaHistogramBoolean("Privacy.Settings.PrivacyGuide.CanShowNTPPromo",
                                can_execute);
      break;
    case Command::kStartTabGroupTutorial:
      can_execute = !!GetTutorialService() && BrowserSupportsTabGroups();
      break;
    case Command::kOpenPasswordManager:
      can_execute = true;
      break;
    case Command::kNoOpCommand:
      can_execute = true;
      break;
    case Command::kOpenPerformanceSettings:
      can_execute = true;
      break;
    case Command::kOpenNTPAndStartCustomizeChromeTutorial:
      can_execute = !!GetTutorialService() &&
                    BrowserSupportsCustomizeChromeSidePanel() &&
                    DefaultSearchProviderIsGoogle();
      break;
    case Command::kStartPasswordManagerTutorial:
      can_execute =
          !!GetTutorialService() && BrowserSupportsNewPasswordManager();
      break;
  }
  std::move(callback).Run(can_execute);
}

void BrowserCommandHandler::ExecuteCommand(Command command_id,
                                           ClickInfoPtr click_info,
                                           ExecuteCommandCallback callback) {
  if (!base::Contains(supported_commands_, command_id)) {
    std::move(callback).Run(false);
    return;
  }

  const auto disposition = ui::DispositionFromClick(
      click_info->middle_button, click_info->alt_key, click_info->ctrl_key,
      click_info->meta_key, click_info->shift_key);
  const bool command_executed =
      GetCommandUpdater()->ExecuteCommandWithDisposition(
          static_cast<int>(command_id), disposition);
  std::move(callback).Run(command_executed);
}

void BrowserCommandHandler::ExecuteCommandWithDisposition(
    int id,
    WindowOpenDisposition disposition) {
  const auto command = static_cast<Command>(id);
  base::UmaHistogramEnumeration(kPromoBrowserCommandHistogramName, command);

  switch (command) {
    case Command::kUnknownCommand:
      // Nothing to do.
      break;
    case Command::kOpenSafetyCheck:
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kSafetyCheckSubPage)),
                    disposition);
      base::RecordAction(
          base::UserMetricsAction("NewTabPage_Promos_SafetyCheck"));
      break;
    case Command::kOpenSafeBrowsingEnhancedProtectionSettings:
      NavigateToURL(GURL(chrome::GetSettingsUrl(
                        chrome::kSafeBrowsingEnhancedProtectionSubPage)),
                    disposition);
      base::RecordAction(
          base::UserMetricsAction("NewTabPage_Promos_EnhancedProtection"));
      break;
    case Command::kOpenFeedbackForm:
      OpenFeedbackForm();
      break;
    case Command::kOpenPrivacyGuide:
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kPrivacyGuideSubPage)),
                    disposition);
      base::RecordAction(
          base::UserMetricsAction("NewTabPage_Promos_PrivacyGuide"));
      break;
    case Command::kStartTabGroupTutorial:
      StartTabGroupTutorial();
      break;
    case Command::kOpenPasswordManager:
      OpenPasswordManager();
      break;
    case Command::kNoOpCommand:
      // Nothing to do.
      break;
    case Command::kOpenPerformanceSettings:
      NavigateToURL(GURL(chrome::GetSettingsUrl(chrome::kPerformanceSubPage)),
                    disposition);
      break;
    case Command::kOpenNTPAndStartCustomizeChromeTutorial:
      OpenNTPAndStartCustomizeChromeTutorial(disposition);
      break;
    case Command::kStartPasswordManagerTutorial:
      StartPasswordManagerTutorial();
      break;
    default:
      NOTREACHED() << "Unspecified behavior for command " << id;
      break;
  }
}

user_education::TutorialService* BrowserCommandHandler::GetTutorialService() {
  auto* service = UserEducationServiceFactory::GetForProfile(profile_);
  return service ? &service->tutorial_service() : nullptr;
}

ui::ElementContext BrowserCommandHandler::GetUiElementContext() {
  return chrome::FindBrowserWithProfile(profile_)
      ->window()
      ->GetElementContext();
}

bool BrowserCommandHandler::BrowserSupportsTabGroups() {
  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  return browser->tab_strip_model()->SupportsTabGroups();
}

bool BrowserCommandHandler::BrowserHasTabGroups() {
  Browser* browser = chrome::FindBrowserWithProfile(profile_);
  return !browser->tab_strip_model()->group_model()->ListTabGroups().empty();
}

void BrowserCommandHandler::StartTabGroupTutorial() {
  user_education::TutorialService* tutorial_service = GetTutorialService();

  // Should never happen since we return false in CanExecuteCommand(), but
  // avoid a browser crash anyway.
  if (!tutorial_service)
    return;

  const ui::ElementContext context = GetUiElementContext();
  if (!context)
    return;

  if (!BrowserSupportsTabGroups()) {
    return;
  }

  user_education::TutorialIdentifier tutorial_id =
      BrowserHasTabGroups() ? kTabGroupWithExistingGroupTutorialId
                            : kTabGroupTutorialId;

  tutorial_service->StartTutorial(tutorial_id, context);
  tutorial_service->LogStartedFromWhatsNewPage(
      tutorial_id, tutorial_service->IsRunningTutorial());
}

void BrowserCommandHandler::OpenPasswordManager() {
  chrome::ShowPasswordManager(chrome::FindBrowserWithProfile(profile_));
}

bool BrowserCommandHandler::BrowserSupportsCustomizeChromeSidePanel() {
  return base::FeatureList::IsEnabled(ntp_features::kCustomizeChromeSidePanel);
}

bool BrowserCommandHandler::DefaultSearchProviderIsGoogle() {
  return search::DefaultSearchProviderIsGoogle(profile_);
}

void BrowserCommandHandler::OpenNTPAndStartCustomizeChromeTutorial(
    WindowOpenDisposition disposition) {
  user_education::TutorialService* tutorial_service = GetTutorialService();

  // Should never happen since we return false in CanExecuteCommand(), but
  // avoid a browser crash anyway.
  if (!tutorial_service) {
    return;
  }

  const ui::ElementContext context = GetUiElementContext();
  if (!context) {
    return;
  }

  if (!BrowserSupportsCustomizeChromeSidePanel()) {
    return;
  }

  if (!DefaultSearchProviderIsGoogle()) {
    return;
  }

  user_education::TutorialIdentifier tutorial_id =
      kSidePanelCustomizeChromeTutorialId;

  tutorial_service->StartTutorial(tutorial_id, context);
  tutorial_service->LogStartedFromWhatsNewPage(
      tutorial_id, tutorial_service->IsRunningTutorial());

  NavigateToURL(GURL(chrome::kChromeUINewTabPageURL), disposition);
}

bool BrowserCommandHandler::BrowserSupportsNewPasswordManager() {
  return base::FeatureList::IsEnabled(
      password_manager::features::kPasswordManagerRedesign);
}

void BrowserCommandHandler::StartPasswordManagerTutorial() {
  user_education::TutorialService* tutorial_service = GetTutorialService();

  // Should never happen since we return false in CanExecuteCommand(), but
  // avoid a browser crash anyway.
  if (!tutorial_service) {
    return;
  }

  const ui::ElementContext context = GetUiElementContext();
  if (!context) {
    return;
  }

  if (!BrowserSupportsNewPasswordManager()) {
    return;
  }

  user_education::TutorialIdentifier tutorial_id = kPasswordManagerTutorialId;

  tutorial_service->StartTutorial(tutorial_id, context);
  tutorial_service->LogStartedFromWhatsNewPage(
      tutorial_id, tutorial_service->IsRunningTutorial());
}

void BrowserCommandHandler::OpenFeedbackForm() {
  chrome::ShowFeedbackPage(feedback_settings_.url, profile_,
                           feedback_settings_.source,
                           std::string() /* description_template */,
                           std::string() /* description_placeholder_text */,
                           feedback_settings_.category /* category_tag */,
                           std::string() /* extra_diagnostics */);
}

void BrowserCommandHandler::ConfigureFeedbackCommand(
    FeedbackCommandSettings settings) {
  feedback_settings_ = settings;
}

void BrowserCommandHandler::EnableSupportedCommands() {
  // Explicitly enable supported commands.
  GetCommandUpdater()->UpdateCommandEnabled(
      static_cast<int>(Command::kUnknownCommand), true);
  for (Command command : supported_commands_) {
    GetCommandUpdater()->UpdateCommandEnabled(static_cast<int>(command), true);
  }
}

CommandUpdater* BrowserCommandHandler::GetCommandUpdater() {
  return command_updater_.get();
}

void BrowserCommandHandler::NavigateToURL(const GURL& url,
                                          WindowOpenDisposition disposition) {
  NavigateParams params(profile_, url, ui::PAGE_TRANSITION_LINK);
  params.disposition = disposition;
  Navigate(&params);
}
