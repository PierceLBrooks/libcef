// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/commerce/price_insights_icon_view.h"

#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/commerce/price_tracking/mock_shopping_list_ui_tab_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/mock_shopping_service.h"
#include "components/commerce/core/test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/interactive_test.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kShoppingTab);

const char kShoppingURL[] = "/shopping.html";
const char kProductClusterTitle[] = "Product Cluster Title";

std::unique_ptr<net::test_server::HttpResponse> BasicResponse(
    const net::test_server::HttpRequest& request) {
  auto response = std::make_unique<net::test_server::BasicHttpResponse>();
  response->set_content("shopping page");
  response->set_content_type("text/html");
  return response;
}
}  // namespace

class PriceInsightsIconViewInteractiveTest : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    set_open_about_blank_on_browser_launch(true);
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->RegisterDefaultHandler(
        base::BindRepeating(&BasicResponse));
    embedded_test_server()->StartAcceptingConnections();

    InteractiveBrowserTest::SetUpOnMainThread();

    SetUpTabHelperAndShoppingService();
  }

 protected:
  raw_ptr<commerce::MockShoppingService, DanglingUntriaged>
      mock_shopping_service_;

 private:
  base::test::ScopedFeatureList test_features_{commerce::kPriceInsights};

  void SetUpTabHelperAndShoppingService() {
    // Remove the original tab helper so we don't get into a bad situation when
    // we go to replace the shopping service with the mock one. The old tab
    // helper is still holding a reference to the original shopping service and
    // other dependencies which we switch out below (leaving some dangling
    // pointers on destruction).
    browser()->tab_strip_model()->GetActiveWebContents()->RemoveUserData(
        commerce::ShoppingListUiTabHelper::UserDataKey());

    mock_shopping_service_ = static_cast<commerce::MockShoppingService*>(
        commerce::ShoppingServiceFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                browser()->profile(),
                base::BindRepeating([](content::BrowserContext* context) {
                  return commerce::MockShoppingService::Build();
                })));

    MockShoppingListUiTabHelper::CreateForWebContents(
        browser()->tab_strip_model()->GetActiveWebContents());
    MockShoppingListUiTabHelper* mock_tab_helper =
        static_cast<MockShoppingListUiTabHelper*>(
            MockShoppingListUiTabHelper::FromWebContents(
                browser()->tab_strip_model()->GetActiveWebContents()));
    EXPECT_CALL(*mock_tab_helper, ShouldShowPriceInsightsIconView)
        .Times(testing::AnyNumber());
    ON_CALL(*mock_tab_helper, ShouldShowPriceInsightsIconView)
        .WillByDefault(testing::Return(true));

    mock_tab_helper->SetShoppingServiceForTesting(mock_shopping_service_);
    mock_shopping_service_->SetIsPriceInsightsEligible(true);

    MockGetProductInfoForUrlResponse();
    MockGetPriceInsightsInfoForUrlResponse();
  }

  void MockGetProductInfoForUrlResponse() {
    commerce::ProductInfo info;
    info.product_cluster_title = kProductClusterTitle;
    mock_shopping_service_->SetResponseForGetProductInfoForUrl(info);
  }

  void MockGetPriceInsightsInfoForUrlResponse() {
    absl::optional<commerce::PriceInsightsInfo> price_insights_info =
        commerce::CreateValidPriceInsightsInfo(
            true, true, commerce::PriceBucket::kLowPrice);
    mock_shopping_service_->SetResponseForGetPriceInsightsInfoForUrl(
        price_insights_info);
  }
};

IN_PROC_BROWSER_TEST_F(PriceInsightsIconViewInteractiveTest,
                       SidePanelShownOnPress) {
  EXPECT_CALL(*mock_shopping_service_, GetProductInfoForUrl);
  EXPECT_CALL(*mock_shopping_service_, GetPriceInsightsInfoForUrl);

  RunTestSequence(
      InstrumentTab(kShoppingTab),
      NavigateWebContents(kShoppingTab,
                          embedded_test_server()->GetURL(kShoppingURL)),
      FlushEvents(),
      // Ensure the side panel isn't open
      EnsureNotPresent(kSidePanelElementId),
      // Click on the action chip to open the side panel
      PressButton(kPriceInsightsChipElementId),
      WaitForShow(kSidePanelElementId), FlushEvents());
}
