// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';
import './profile_creation_shared.css.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {WebUiListenerMixin} from 'chrome://resources/cr_elements/web_ui_listener_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/focus_without_ink.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
import {sanitizeInnerHtml} from 'chrome://resources/js/parse_html_subset.js';
import {afterNextRender, DomRepeatEvent, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AutogeneratedThemeColorInfo, AvailableAccount, ManageProfilesBrowserProxy, ManageProfilesBrowserProxyImpl} from '../manage_profiles_browser_proxy.js';
import {hasPreviousRoute, navigateToPreviousRoute} from '../navigation_mixin.js';

import {getTemplate} from './account_selection_lacros.html.js';

export interface AccountSelectionLacrosElement {
  $: {
    backButton: HTMLElement,
    'product-logo': HTMLElement,
  };
}

const AccountSelectionLacrosElementBase =
    WebUiListenerMixin(I18nMixin(PolymerElement));

export class AccountSelectionLacrosElement extends
    AccountSelectionLacrosElementBase {
  static get is() {
    return 'account-selection-lacros';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      profileThemeInfo: Object,

      availableAccounts_: {
        type: Array,
        value: () => [],
      },

      hasPreviousRoute_: {
        type: Boolean,
        value: () => hasPreviousRoute(),
      },

      accountSelected_: {
        type: Boolean,
        value: false,
      },
    };
  }

  private manageProfilesBrowserProxy_: ManageProfilesBrowserProxy =
      ManageProfilesBrowserProxyImpl.getInstance();

  profileThemeInfo: AutogeneratedThemeColorInfo;
  private availableAccounts_: AvailableAccount[];
  private accountSelected_: boolean;

  override connectedCallback() {
    super.connectedCallback();
    this.addWebUiListener(
        'available-accounts-changed',
        (accounts: AvailableAccount[]) =>
            this.handleAvailableAccountsChanged_(accounts));
    this.manageProfilesBrowserProxy_.getAvailableAccounts();

    this.addWebUiListener(
        'reauth-dialog-closed', () => this.accountSelected_ = false);
  }

  override ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);

    const guestModeLink = this.shadowRoot!.querySelector('#guestModeLink');
    if (guestModeLink) {
      guestModeLink.addEventListener(
          'click', (e: Event) => this.openGuestLink_(e));
    }
  }

  private onViewEnterStart_() {
    if (hasPreviousRoute()) {
      afterNextRender(
          this,
          () =>
              focusWithoutInk(this.shadowRoot!.querySelector('#backButton')!));
    }
  }

  private onBackClick_() {
    assert(hasPreviousRoute());
    navigateToPreviousRoute();
  }

  private getBackButtonAriaLabel_(): string {
    return this.i18n(
        'backButtonAriaLabel', this.i18n('accountSelectionLacrosTitle'));
  }

  private getSubtitle_(): TrustedHTML {
    return sanitizeInnerHtml(
        loadTimeData.getString('accountSelectionLacrosSubtitle'), {
          attrs: ['id'],
        });
  }

  private onProductLogoClick_() {
    this.$['product-logo'].animate(
        {
          transform: ['none', 'rotate(-10turn)'],
        },
        {
          duration: 500,
          easing: 'cubic-bezier(1, 0, 0, 1)',
        });
  }

  private onAccountClick_(e: DomRepeatEvent<AvailableAccount>) {
    this.accountSelected_ = true;
    const gaiaId = e.model.item.gaiaId;
    this.manageProfilesBrowserProxy_.selectExistingAccountLacros(
        this.profileThemeInfo.color, gaiaId);
  }

  private onOtherAccountClick_() {
    this.manageProfilesBrowserProxy_.selectNewAccount(
        this.profileThemeInfo.color);
  }

  private openGuestLink_(e: Event) {
    e.preventDefault();  // Block the navigation.
    this.manageProfilesBrowserProxy_.openDeviceGuestLinkLacros();
  }

  private handleAvailableAccountsChanged_(availableAccounts:
                                              AvailableAccount[]) {
    this.availableAccounts_ = availableAccounts;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'account-selection-lacros': AccountSelectionLacrosElement;
  }
}

customElements.define(
    AccountSelectionLacrosElement.is, AccountSelectionLacrosElement);
