// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_tracking_view.h"

#include "base/metrics/user_metrics.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/omnibox/browser/vector_icons.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets_outsets_base.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/toggle_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_provider.h"

namespace {
constexpr int kProductImageSize = 56;
constexpr int kImageBorderRadius = 4;
constexpr int kIconSize = 16;
constexpr int kIconMargin = 14;
constexpr int kIconSizeRefresh = 20;
constexpr int kIconMarginRefresh = 8;

int GetIconMargin() {
  return features::IsChromeRefresh2023() ? kIconMarginRefresh : kIconMargin;
}

gfx::Size GetIconSize() {
  return features::IsChromeRefresh2023()
             ? gfx::Size(kIconSizeRefresh, kIconSizeRefresh)
             : gfx::Size(kIconSize, kIconSize);
}

}  // namespace

PriceTrackingView::PriceTrackingView(Profile* profile,
                                     const GURL& page_url,
                                     const gfx::ImageSkia& product_image,
                                     bool is_price_track_enabled,
                                     const commerce::ProductInfo& product_info)
    : profile_(profile),
      is_price_track_enabled_(is_price_track_enabled),
      product_info_(product_info) {
  auto* layout_provider = views::LayoutProvider::Get();
  const bool power_bookmarks_side_panel_enabled =
      base::FeatureList::IsEnabled(features::kPowerBookmarksSidePanel);
  const int label_spacing = power_bookmarks_side_panel_enabled ? 0 : 4;
  const auto label_context = power_bookmarks_side_panel_enabled
                                 ? views::style::CONTEXT_LABEL
                                 : views::style::CONTEXT_DIALOG_BODY_TEXT;
  const int horizontal_spacing = layout_provider->GetDistanceMetric(
      views::DISTANCE_RELATED_CONTROL_HORIZONTAL);
  const gfx::Insets dialog_insets =
      layout_provider->GetInsetsMetric(views::INSETS_DIALOG);
  if (power_bookmarks_side_panel_enabled) {
    if (features::IsChromeRefresh2023()) {
      SetCrossAxisAlignment(views::LayoutAlignment::kStart);
    }
    // Icon column
    auto* icon = AddChildView(std::make_unique<views::ImageView>());
    icon->SetImage(
        ui::ImageModel::FromVectorIcon(omnibox::kPriceTrackingDisabledIcon,
                                       kColorBookmarkDialogTrackPriceIcon));
    icon->SetImageSize(GetIconSize());
    icon->SetProperty(views::kMarginsKey,
                      gfx::Insets::TLBR(0, 0, 0, GetIconMargin()));
  } else {
    // image column
    auto* product_image_containter =
        AddChildView(std::make_unique<views::BoxLayoutView>());
    product_image_containter->SetCrossAxisAlignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);
    product_image_containter->SetProperty(
        views::kMarginsKey, gfx::Insets::TLBR(0, 0, 0, horizontal_spacing));
    // Set product image.
    product_image_containter->AddChildView(
        views::Builder<views::ImageView>()
            .SetImageSize(gfx::Size(kProductImageSize, kProductImageSize))
            .SetPreferredSize(gfx::Size(kProductImageSize, kProductImageSize))
            // TODO(meiliang@): Verify color and corner radius with UX.
            .SetBorder(views::CreateThemedRoundedRectBorder(
                1, kImageBorderRadius, kColorBookmarkDialogProductImageBorder))
            .SetImage(
                gfx::ImageSkiaOperations::CreateCroppedCenteredRoundRectImage(
                    gfx::Size(kProductImageSize, kProductImageSize),
                    kImageBorderRadius, product_image))
            .Build());
  }

  // Text column
  auto text_container = std::make_unique<views::FlexLayoutView>();
  text_container->SetOrientation(views::LayoutOrientation::kVertical);
  // Title label
  auto* title_label =
      text_container->AddChildView(std::make_unique<views::Label>(
          l10n_util::GetStringUTF16(IDS_OMNIBOX_TRACK_PRICE_DIALOG_TITLE),
          label_context,
          features::IsChromeRefresh2023() ? views::style::STYLE_EMPHASIZED
                                          : views::style::STYLE_PRIMARY));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
  // Body label
  int body_string_id = IDS_BOOKMARK_STAR_DIALOG_TRACK_PRICE_DESCRIPTION;
  if (profile_ && commerce::IsEmailDisabledByUser(profile_->GetPrefs())) {
    body_string_id = IDS_BOOKMARK_STAR_DIALOG_TRACK_PRICE_DESCRIPTION_EMAIL_OFF;
  }
  body_label_ = text_container->AddChildView(std::make_unique<views::Label>(
      l10n_util::GetStringUTF16(body_string_id), label_context,
      views::style::STYLE_SECONDARY));
  if (power_bookmarks_side_panel_enabled) {
    body_label_->SetFontList(body_label_->font_list().DeriveWithSizeDelta(-1));
  }
  body_label_->SetProperty(views::kMarginsKey,
                           gfx::Insets::TLBR(label_spacing, 0, 0, 0));
  body_label_->SetMultiLine(true);
  body_label_->SetAllowCharacterBreak(true);
  body_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  body_label_->SetFocusBehavior(View::FocusBehavior::ACCESSIBLE_ONLY);
  AddChildView(std::move(text_container));

  // Toggle button column
  toggle_button_ = AddChildView(std::make_unique<views::ToggleButton>(
      base::BindRepeating(&PriceTrackingView::OnToggleButtonPressed,
                          weak_ptr_factory_.GetWeakPtr(), page_url)));

  toggle_button_->SetIsOn(is_price_track_enabled_);

  scoped_observation_.Observe(
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile_));

  toggle_button_->SetAccessibleName(GetToggleAccessibleName());
  toggle_button_->SetProperty(views::kMarginsKey,
                              gfx::Insets::TLBR(0, horizontal_spacing, 0, 0));

  const int bubble_width = ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  int label_width;
  if (power_bookmarks_side_panel_enabled) {
    toggle_button_->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kPreferred,
                                 features::IsChromeRefresh2023()
                                     ? views::MaximumFlexSizeRule::kPreferred
                                     : views::MaximumFlexSizeRule::kUnbounded)
            .WithAlignment(views::LayoutAlignment::kEnd));

    label_width = bubble_width - horizontal_spacing * 2 -
                  dialog_insets.right() - GetIconMargin() -
                  GetIconSize().width() -
                  toggle_button_->GetPreferredSize().width();
  } else {
    label_width = bubble_width - horizontal_spacing * 2 -
                  dialog_insets.width() - kProductImageSize -
                  toggle_button_->GetPreferredSize().width();
  }
  body_label_->SizeToFit(label_width);
  base::RecordAction(base::UserMetricsAction(
      "Commerce.PriceTracking.BookmarkDialogPriceTrackViewShown"));
}

PriceTrackingView::~PriceTrackingView() = default;

bool PriceTrackingView::IsToggleOn() {
  return toggle_button_->GetIsOn();
}

void PriceTrackingView::OnSubscribe(const commerce::CommerceSubscription& sub,
                                    bool succeeded) {
  if (succeeded) {
    HandleSubscriptionUpdate(sub, true);
  }
}

void PriceTrackingView::OnUnsubscribe(const commerce::CommerceSubscription& sub,
                                      bool succeeded) {
  if (succeeded) {
    HandleSubscriptionUpdate(sub, false);
  }
}

void PriceTrackingView::HandleSubscriptionUpdate(
    const commerce::CommerceSubscription& sub,
    bool is_tracking) {
  if (sub.id_type == commerce::IdentifierType::kProductClusterId &&
      base::NumberToString(product_info_.product_cluster_id.value_or(
          commerce::kInvalidSubscriptionId)) == sub.id) {
    is_price_track_enabled_ = is_tracking;
    toggle_button_->SetIsOn(is_tracking);
    toggle_button_->SetAccessibleName(GetToggleAccessibleName());
  }
}

std::u16string PriceTrackingView::GetToggleAccessibleName() {
  return l10n_util::GetStringUTF16(
      IsToggleOn() ? IDS_PRICE_TRACKING_UNTRACK_PRODUCT_ACCESSIBILITY
                   : IDS_PRICE_TRACKING_TRACK_PRODUCT_ACCESSIBILITY);
}

void PriceTrackingView::OnToggleButtonPressed(const GURL& url) {
  is_price_track_enabled_ = !is_price_track_enabled_;
  if (is_price_track_enabled_) {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.BookmarkDialogPriceTrackViewTrackedPrice"));
  } else {
    base::RecordAction(base::UserMetricsAction(
        "Commerce.PriceTracking.BookmarkDialogPriceTrackViewUntrackedPrice"));
  }

  toggle_button_->SetAccessibleName(GetToggleAccessibleName());
  UpdatePriceTrackingState(url);
}

void PriceTrackingView::UpdatePriceTrackingState(const GURL& url) {
  bookmarks::BookmarkModel* const model =
      BookmarkModelFactory::GetForBrowserContext(profile_);
  const bookmarks::BookmarkNode* node =
      model->GetMostRecentlyAddedUserNodeForURL(url);

  // If "track by default" is on, we'll show a dialog after saving to offer
  // email notifications.
  if (!base::FeatureList::IsEnabled(commerce::kShoppingListTrackByDefault) &&
      profile_ && is_price_track_enabled_) {
    commerce::MaybeEnableEmailNotifications(profile_->GetPrefs());
  }

  commerce::ShoppingService* service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile_);
  base::OnceCallback<void(bool)> callback =
      base::BindOnce(&PriceTrackingView::OnPriceTrackingStateUpdated,
                     weak_ptr_factory_.GetWeakPtr());

  if (node) {
    commerce::SetPriceTrackingStateForBookmark(
        service, model, node, is_price_track_enabled_, std::move(callback));
  } else {
    DCHECK(!is_price_track_enabled_);
    if (commerce::CanTrackPrice(product_info_)) {
      commerce::SetPriceTrackingStateForClusterId(
          service, model, product_info_.product_cluster_id.value(),
          is_price_track_enabled_, std::move(callback));
    }
  }
}

void PriceTrackingView::OnPriceTrackingStateUpdated(bool success) {
  // TODO(crbug.com/1346612): Record latency for the update status.
  if (!success) {
    is_price_track_enabled_ = !is_price_track_enabled_;
    toggle_button_->SetIsOn(is_price_track_enabled_);
    toggle_button_->SetAccessibleName(GetToggleAccessibleName());
    body_label_->SetText(l10n_util::GetStringUTF16(
        IDS_OMNIBOX_TRACK_PRICE_DIALOG_ERROR_DESCRIPTION));
  }
}
