// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/companion_page_handler.h"

#include "build/build_config.h"
#include "chrome/browser/companion/core/companion_metrics_logger.h"
#include "chrome/browser/companion/core/companion_permission_utils.h"
#include "chrome/browser/companion/core/companion_url_builder.h"
#include "chrome/browser/companion/core/features.h"
#include "chrome/browser/companion/core/promo_handler.h"
#include "chrome/browser/companion/text_finder/text_finder_manager.h"
#include "chrome/browser/companion/text_finder/text_highlighter_manager.h"
#include "chrome/browser/companion/visual_search/features.h"
#include "chrome/browser/companion/visual_search/visual_search_suggestions_service_factory.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/side_panel/companion/companion_side_panel_controller_utils.h"
#include "chrome/browser/ui/side_panel/companion/companion_tab_helper.h"
#include "chrome/browser/ui/side_panel/companion/companion_utils.h"
#include "chrome/browser/ui/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_side_panel_untrusted_ui.h"
#include "chrome/browser/ui/webui/side_panel/companion/signin_delegate_impl.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/webui_url_constants.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/lens/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/unified_consent_service.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

namespace companion {

CompanionPageHandler::CompanionPageHandler(
    mojo::PendingReceiver<side_panel::mojom::CompanionPageHandler> receiver,
    mojo::PendingRemote<side_panel::mojom::CompanionPage> page,
    CompanionSidePanelUntrustedUI* companion_untrusted_ui)
    : receiver_(this, std::move(receiver)),
      page_(std::move(page)),
      companion_untrusted_ui_(companion_untrusted_ui),
      signin_delegate_(std::make_unique<SigninDelegateImpl>(
          companion_untrusted_ui_->web_ui()->GetWebContents())),
      url_builder_(
          std::make_unique<CompanionUrlBuilder>(GetProfile()->GetPrefs(),
                                                signin_delegate_.get())),
      promo_handler_(std::make_unique<PromoHandler>(GetProfile()->GetPrefs(),
                                                    signin_delegate_.get())),
      consent_helper_(unified_consent::UrlKeyedDataCollectionConsentHelper::
                          NewAnonymizedDataCollectionConsentHelper(
                              GetProfile()->GetPrefs())) {
  identity_manager_observation_.Observe(
      IdentityManagerFactory::GetForProfile(GetProfile()));
  consent_helper_observation_.Observe(consent_helper_.get());
  if (base::FeatureList::IsEnabled(
          visual_search::features::kVisualSearchSuggestions)) {
    visual_search_host_ =
        std::make_unique<visual_search::VisualSearchClassifierHost>(
            visual_search::VisualSearchSuggestionsServiceFactory::GetForProfile(
                GetProfile()));
  }
}

CompanionPageHandler::~CompanionPageHandler() {
  if (web_contents() && !web_contents()->IsBeingDestroyed()) {
    auto* tab_helper =
        companion::CompanionTabHelper::FromWebContents(web_contents());
    tab_helper->OnCompanionSidePanelClosed();
  }
}

void CompanionPageHandler::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  // We only care about the sign-in state changes. Sync state change is already
  // captured through consent helper observer.
  if (event_details.GetEventTypeFor(signin::ConsentLevel::kSignin) ==
      signin::PrimaryAccountChangeEvent::Type::kNone) {
    return;
  }

  NotifyURLChanged(/*is_full_reload=*/true);
}

void CompanionPageHandler::OnErrorStateOfRefreshTokenUpdatedForAccount(
    const CoreAccountInfo& account_info,
    const GoogleServiceAuthError& error) {
  NotifyURLChanged(/*is_full_reload=*/true);
}

void CompanionPageHandler::OnUrlKeyedDataCollectionConsentStateChanged(
    unified_consent::UrlKeyedDataCollectionConsentHelper* consent_helper) {
  NotifyURLChanged(/*is_full_reload=*/true);
}

void CompanionPageHandler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  bool is_reload_or_explicit_navigation =
      (navigation_handle->GetReloadType() != content::ReloadType::NONE) ||
      (navigation_handle->GetPageTransition() &
       ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

  // If the URL didn't change and it's not a manual reload, no need to refresh
  // the companion.
  if (page_url_.GetWithoutRef() ==
          web_contents()->GetLastCommittedURL().GetWithoutRef() &&
      !is_reload_or_explicit_navigation) {
    return;
  }

  page_url_ = web_contents()->GetLastCommittedURL();

  ukm::SourceId ukm_source_id =
      web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  metrics_logger_ = std::make_unique<CompanionMetricsLogger>(ukm_source_id);
  auto* tab_helper =
      companion::CompanionTabHelper::FromWebContents(web_contents());
  auto open_trigger = tab_helper->GetAndResetMostRecentSidePanelOpenTrigger();
  if (open_trigger.has_value()) {
    metrics_logger_->RecordOpenTrigger(open_trigger);
  }

  // Only notify the companion UI the page changed if we can share
  // information about the page by user consent.
  if (!IsUserPermittedToSharePageInfoWithCompanion(GetProfile()->GetPrefs())) {
    return;
  }
  NotifyURLChanged(/*is_full_reload=*/false);
}

void CompanionPageHandler::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // We only want to classify images in the main frame.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    return;
  }

  // TODO(b/284640445) - Add browser test to verify side effect of feature
  // on/off, use histogram check to determine whether or not classification was
  // called.
  if (visual_search_host_) {
    visual_search::VisualSearchClassifierHost::ResultCallback callback =
        base::BindOnce(&CompanionPageHandler::HandleVisualSearchResult,
                       weak_ptr_factory_.GetWeakPtr());
    visual_search_host_->StartClassification(render_frame_host, validated_url,
                                             std::move(callback));
  }
}

void CompanionPageHandler::HandleVisualSearchResult(
    std::vector<std::string> results) {
  std::vector<side_panel::mojom::VisualSearchResultPtr> final_results;
  for (const auto& result : results) {
    final_results.emplace_back(
        side_panel::mojom::VisualSearchResult::New(result));
  }
  if (!final_results.empty()) {
    page_->OnDeviceVisualClassificationResult(std::move(final_results));
  }
}

void CompanionPageHandler::ShowUI() {
  if (auto embedder = companion_untrusted_ui_->embedder()) {
    embedder->ShowUI();

    // Calls to the browser need to happen after the ShowUI() call above since
    // it is only added to browser hierarchy after the side panel has loaded the
    // page.
    auto* browser = GetBrowser();
    if (!browser) {
      return;
    }

    auto* active_web_contents =
        GetBrowser()->tab_strip_model()->GetActiveWebContents();
    Observe(active_web_contents);
    page_url_ = active_web_contents->GetLastCommittedURL();
    ukm::SourceId ukm_source_id =
        web_contents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
    metrics_logger_ = std::make_unique<CompanionMetricsLogger>(ukm_source_id);
    auto* helper =
        companion::CompanionTabHelper::FromWebContents(active_web_contents);
    helper->SetCompanionPageHandler(weak_ptr_factory_.GetWeakPtr());
    metrics_logger_->RecordOpenTrigger(
        helper->GetAndResetMostRecentSidePanelOpenTrigger());

    // Register a modal dialog manager to show permissions dialog like those
    // requested from the feedback UI.
    RegisterModalDialogManager(browser);
    std::string initial_text_query = helper->GetTextQuery();
    if (!initial_text_query.empty()) {
      OnSearchTextQuery(initial_text_query);
      return;
    }

    std::unique_ptr<side_panel::mojom::ImageQuery> image_query =
        helper->GetImageQuery();
    if (image_query) {
      OnImageQuery(*image_query);
      return;
    }

    NotifyURLChanged(/*is_full_reload=*/true);
  }
}

void CompanionPageHandler::OnSearchTextQuery(const std::string& query) {
  // Only notify the companion UI the page changed if we can share
  // information about the page by user consent.
  GURL page_url;
  if (IsUserPermittedToSharePageInfoWithCompanion(GetProfile()->GetPrefs())) {
    page_url = web_contents()->GetVisibleURL();
  }

  GURL companion_url = url_builder_->BuildCompanionURL(page_url, query);
  page_->LoadCompanionPage(companion_url);
}

void CompanionPageHandler::NotifyURLChanged(bool is_full_reload) {
  if (is_full_reload) {
    GURL companion_url =
        url_builder_->BuildCompanionURL(web_contents()->GetVisibleURL());
    page_->LoadCompanionPage(companion_url);
  } else {
    auto companion_update_proto = url_builder_->BuildCompanionUrlParamProto(
        web_contents()->GetVisibleURL());
    page_->UpdateCompanionPage(companion_update_proto);
  }
}

void CompanionPageHandler::OnImageQuery(
    side_panel::mojom::ImageQuery image_query) {
  GURL modified_upload_url = url_builder_->AppendCompanionParamsToURL(
      image_query.upload_url, web_contents()->GetVisibleURL(),
      /*text_query=*/"");
  image_query.upload_url = modified_upload_url;
  page_->OnImageQuery(image_query.Clone());
}

void CompanionPageHandler::OnPromoAction(
    side_panel::mojom::PromoType promo_type,
    side_panel::mojom::PromoAction promo_action) {
  if (promo_type == side_panel::mojom::PromoType::kRegionSearchIPH) {
    if (promo_action == side_panel::mojom::PromoAction::kRejected) {
      auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
          GetProfile());
      tracker->Dismissed(
          feature_engagement::kIPHCompanionSidePanelRegionSearchFeature);
    }
    return;
  }

  promo_handler_->OnPromoAction(promo_type, promo_action);
  metrics_logger_->OnPromoAction(promo_type, promo_action);
}

void CompanionPageHandler::OnRegionSearchClicked() {
  auto* helper = companion::CompanionTabHelper::FromWebContents(web_contents());
  CHECK(helper);
  helper->StartRegionSearch(web_contents(), /*use_fullscreen_capture=*/false);
  metrics_logger_->RecordUiSurfaceClicked(
      side_panel::mojom::UiSurface::kRegionSearch, kInvalidPosition);
  feature_engagement::TrackerFactory::GetForBrowserContext(GetProfile())
      ->NotifyEvent("companion_side_panel_region_search_button_clicked");
}

void CompanionPageHandler::OnExpsOptInStatusAvailable(bool is_exps_opted_in) {
  metrics_logger_->OnExpsOptInStatusAvailable(is_exps_opted_in);
  auto* pref_service = GetProfile()->GetPrefs();
  pref_service->SetBoolean(kExpsOptInStatusGrantedPref, is_exps_opted_in);
  // Update default value for pref indicating whether companion should be
  // pinned to the toolbar.
  companion::UpdateCompanionDefaultPinnedToToolbarState(pref_service);
}

void CompanionPageHandler::OnOpenInNewTabButtonURLChanged(
    const GURL& url_to_open) {
  auto* companion_helper =
      companion::CompanionTabHelper::FromWebContents(web_contents());
  DCHECK(companion_helper);
  companion_helper->UpdateNewTabButton(url_to_open);
}

void CompanionPageHandler::RecordUiSurfaceShown(
    side_panel::mojom::UiSurface ui_surface,
    uint32_t ui_surface_position,
    uint32_t child_element_available_count,
    uint32_t child_element_shown_count) {
  metrics_logger_->RecordUiSurfaceShown(ui_surface, ui_surface_position,
                                        child_element_available_count,
                                        child_element_shown_count);
}

void CompanionPageHandler::RecordUiSurfaceClicked(
    side_panel::mojom::UiSurface ui_surface,
    int32_t click_position) {
  metrics_logger_->RecordUiSurfaceClicked(ui_surface, click_position);
}

void CompanionPageHandler::OnCqCandidatesAvailable(
    const std::vector<std::string>& text_directives) {
  auto* text_finder_manager =
      TextFinderManager::GetOrCreateForPage(web_contents()->GetPrimaryPage());
  CHECK(text_finder_manager);
  text_finder_manager->CreateTextFinders(
      text_directives,
      base::BindOnce(&CompanionPageHandler::DidFinishFindingCqTexts,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CompanionPageHandler::OnPhFeedback(
    side_panel::mojom::PhFeedback ph_feedback) {
  metrics_logger_->OnPhFeedback(ph_feedback);
}

void CompanionPageHandler::OnCqJumptagClicked(
    const std::string& text_directive) {
  auto* text_highlighter_manager = TextHighlighterManager::GetOrCreateForPage(
      web_contents()->GetPrimaryPage());
  text_highlighter_manager->CreateTextHighlighterAndRemoveExistingInstance(
      text_directive);
}

void CompanionPageHandler::OpenUrlInBrowser(
    const absl::optional<GURL>& url_to_open,
    bool use_new_tab) {
  if (!url_to_open.has_value() || !url_to_open.value().is_valid()) {
    return;
  }

  signin_delegate_->OpenUrlInBrowser(url_to_open.value(), use_new_tab);
}

Browser* CompanionPageHandler::GetBrowser() {
  auto* webui_contents = companion_untrusted_ui_->web_ui()->GetWebContents();
  auto* browser = companion::GetBrowserForWebContents(webui_contents);
  return browser;
}

Profile* CompanionPageHandler::GetProfile() {
  CHECK(companion_untrusted_ui_);
  return Profile::FromWebUI(companion_untrusted_ui_->web_ui());
}

void CompanionPageHandler::DidFinishFindingCqTexts(
    const std::vector<std::pair<std::string, bool>>& text_found_vec) {
  std::vector<std::string> text_directives(text_found_vec.size(), "");
  std::vector<bool> find_results(text_found_vec.size(), false);
  for (size_t i = 0; i < text_found_vec.size(); i++) {
    const auto& text_found = text_found_vec[i];
    text_directives[i] = text_found.first;
    find_results[i] = text_found.second;
  }
  page_->OnCqFindTextResultsAvailable(text_directives, find_results);
}

void CompanionPageHandler::RegisterModalDialogManager(Browser* browser) {
  CHECK(companion_untrusted_ui_);
  web_modal::WebContentsModalDialogManager::CreateForWebContents(
      companion_untrusted_ui_->web_ui()->GetWebContents());
  web_modal::WebContentsModalDialogManager::FromWebContents(
      companion_untrusted_ui_->web_ui()->GetWebContents())
      ->SetDelegate(browser);
}

}  // namespace companion
