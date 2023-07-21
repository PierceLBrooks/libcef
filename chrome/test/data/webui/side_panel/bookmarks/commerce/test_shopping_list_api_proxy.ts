// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ShoppingListApiProxy} from 'chrome://bookmarks-side-panel.top-chrome/shared/commerce/shopping_list_api_proxy.js';
import {BookmarkProductInfo, PageCallbackRouter, PageRemote, ProductInfo} from 'chrome://bookmarks-side-panel.top-chrome/shared/shopping_list.mojom-webui.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export class TestShoppingListApiProxy extends TestBrowserProxy implements
    ShoppingListApiProxy {
  callbackRouter: PageCallbackRouter;
  callbackRouterRemote: PageRemote;
  private products_: BookmarkProductInfo[] = [];
  private product_: ProductInfo = {
    title: 'Product Foo',
    domain: 'foo.com',
    imageUrl: {url: 'https://foo.com/image'},
    productUrl: {url: 'https://foo.com/product'},
    currentPrice: '$12',
    previousPrice: '$34',
  };

  constructor() {
    super([
      'getAllPriceTrackedBookmarkProductInfo',
      'getAllShoppingBookmarkProductInfo',
      'trackPriceForBookmark',
      'untrackPriceForBookmark',
      'getProductInfoForCurrentUrl',
    ]);

    this.callbackRouter = new PageCallbackRouter();

    this.callbackRouterRemote =
        this.callbackRouter.$.bindNewPipeAndPassRemote();
  }

  setProducts(products: BookmarkProductInfo[]) {
    this.products_ = products;
  }

  getAllPriceTrackedBookmarkProductInfo() {
    this.methodCalled('getAllPriceTrackedBookmarkProductInfo');
    return Promise.resolve({productInfos: this.products_});
  }

  getAllShoppingBookmarkProductInfo() {
    this.methodCalled('getAllShoppingBookmarkProductInfo');
    return Promise.resolve({productInfos: this.products_});
  }

  trackPriceForBookmark(bookmarkId: bigint) {
    this.methodCalled('trackPriceForBookmark', bookmarkId);
  }

  untrackPriceForBookmark(bookmarkId: bigint) {
    this.methodCalled('untrackPriceForBookmark', bookmarkId);
  }

  getProductInfoForCurrentUrl() {
    this.methodCalled('getProductInfoForCurrentUrl');
    return Promise.resolve({productInfo: this.product_});
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  getCallbackRouterRemote() {
    return this.callbackRouterRemote;
  }
}
