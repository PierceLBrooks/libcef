// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/cr_shared_style.css.js';
import 'chrome://resources/cr_elements/md_select.css.js';
import '/shared/settings/controls/controlled_radio_button.js';
import '/shared/settings/controls/extension_controlled_indicator.js';
import '/shared/settings/controls/settings_radio_group.js';
import '/shared/settings/controls/settings_toggle_button.js';
import '../settings_page/settings_animated_pages.js';
import '../settings_page/settings_subpage.js';
import '../settings_shared.css.js';
import '../settings_vars.css.js';
import './home_url_input.js';
import '/shared/settings/controls/settings_dropdown_menu.js';

import {DropdownMenuOptionList, SettingsDropdownMenuElement} from '/shared/settings/controls/settings_dropdown_menu.js';
import {PrefsMixin} from 'chrome://resources/cr_components/settings_prefs/prefs_mixin.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BaseMixin} from '../base_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {AppearancePageVisibility} from '../page_visibility.js';
import {routes} from '../route.js';
import {Router} from '../router.js';

import {AppearanceBrowserProxy, AppearanceBrowserProxyImpl} from './appearance_browser_proxy.js';
import {getTemplate} from './appearance_page.html.js';


/**
 * This is the absolute difference maintained between standard and
 * fixed-width font sizes. http://crbug.com/91922.
 */
const SIZE_DIFFERENCE_FIXED_STANDARD: number = 3;

/**
 * ID for autogenerated themes. Should match
 * |ThemeService::kAutogeneratedThemeID|.
 */
const AUTOGENERATED_THEME_ID: string = 'autogenerated_theme_id';

/**
 * 'settings-appearance-page' is the settings page containing appearance
 * settings.
 */

export interface SettingsAppearancePageElement {
  $: {
    defaultFontSize: SettingsDropdownMenuElement,
    zoomLevel: HTMLSelectElement,
  };
}

// This must be kept in sync with the SystemTheme enum in
// ui/color/system_theme.h.
export enum SystemTheme {
  // Either classic or web theme.
  DEFAULT = 0,
  // <if expr="is_linux">
  GTK = 1,
  QT = 2,
  // </if>
}

const SettingsAppearancePageElementBase =
    I18nMixin(PrefsMixin(BaseMixin(PolymerElement)));

export class SettingsAppearancePageElement extends
    SettingsAppearancePageElementBase {
  static get is() {
    return 'settings-appearance-page';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Dictionary defining page visibility.
       */
      pageVisibility: Object,

      prefs: {
        type: Object,
        notify: true,
      },

      defaultZoom_: Number,

      isWallpaperPolicyControlled_: {type: Boolean, value: true},

      /**
       * List of options for the font size drop-down menu.
       */
      fontSizeOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {value: 9, name: loadTimeData.getString('verySmall')},
            {value: 12, name: loadTimeData.getString('small')},
            {value: 16, name: loadTimeData.getString('medium')},
            {value: 20, name: loadTimeData.getString('large')},
            {value: 24, name: loadTimeData.getString('veryLarge')},
          ];
        },
      },

      /**
       * Predefined zoom factors to be used when zooming in/out. These are in
       * ascending order. Values are displayed in the page zoom drop-down menu
       * as percentages.
       */
      pageZoomLevels_: Array,

      themeSublabel_: String,

      themeUrl_: String,

      systemTheme_: {
        type: Object,
        value: SystemTheme.DEFAULT,
      },

      focusConfig_: {
        type: Object,
        value() {
          const map = new Map();
          if (routes.FONTS) {
            map.set(routes.FONTS.path, '#customize-fonts-subpage-trigger');
          }
          return map;
        },
      },

      showReaderModeOption_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showReaderModeOption');
        },
      },

      isForcedTheme_: {
        type: Boolean,
        computed: 'computeIsForcedTheme_(' +
            'prefs.autogenerated.theme.policy.color.controlledBy)',
      },

      // <if expr="is_linux">
      /**
       * Whether to show the "Custom Chrome Frame" setting.
       */
      showCustomChromeFrame_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showCustomChromeFrame');
        },
      },
      // </if>

      showSidePanelOptions_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showSidePanelOptions');
        },
      },

      showHoverCardImagesOption_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('showHoverCardImagesOption');
        },
      },

      showManagedThemeDialog_: Boolean,
    };
  }

  static get observers() {
    return [
      'defaultFontSizeChanged_(prefs.webkit.webprefs.default_font_size.value)',
      'themeChanged_(' +
          'prefs.extensions.theme.id.value, systemTheme_, isForcedTheme_)',

      // <if expr="is_linux">
      'systemThemePrefChanged_(prefs.extensions.theme.system_theme.value)',
      // </if>
    ];
  }

  pageVisibility: AppearancePageVisibility;
  private showSidePanelOptions_: boolean;
  private defaultZoom_: number;
  private isWallpaperPolicyControlled_: boolean;
  private fontSizeOptions_: DropdownMenuOptionList;
  private pageZoomLevels_: number[];
  private themeSublabel_: string;
  private themeUrl_: string;
  private systemTheme_: SystemTheme;
  private focusConfig_: Map<string, string>;
  private showReaderModeOption_: boolean;
  private isForcedTheme_: boolean;
  private showHoverCardImagesOption_: boolean;

  // <if expr="is_linux">
  private showCustomChromeFrame_: boolean;
  // </if>

  private showManagedThemeDialog_: boolean;
  private appearanceBrowserProxy_: AppearanceBrowserProxy =
      AppearanceBrowserProxyImpl.getInstance();

  override ready() {
    super.ready();

    this.$.defaultFontSize.menuOptions = this.fontSizeOptions_;
    // TODO(dschuyler): Look into adding a listener for the
    // default zoom percent.
    this.appearanceBrowserProxy_.getDefaultZoom().then(zoom => {
      this.defaultZoom_ = zoom;
    });

    this.pageZoomLevels_ =
        JSON.parse(loadTimeData.getString('presetZoomFactors'));
  }

  /** @return A zoom easier read by users. */
  private formatZoom_(zoom: number): number {
    return Math.round(zoom * 100);
  }

  /**
   * @param showHomepage Whether to show home page.
   * @param isNtp Whether to use the NTP as the home page.
   * @param homepageValue If not using NTP, use this URL.
   */
  private getShowHomeSubLabel_(
      showHomepage: boolean, isNtp: boolean, homepageValue: string): string {
    if (!showHomepage) {
      return this.i18n('homeButtonDisabled');
    }
    if (isNtp) {
      return this.i18n('homePageNtp');
    }
    return homepageValue || this.i18n('customWebAddress');
  }

  private onCustomizeFontsClick_() {
    Router.getInstance().navigateTo(routes.FONTS);
  }

  private onDisableExtension_() {
    this.dispatchEvent(new CustomEvent(
        'refresh-pref', {bubbles: true, composed: true, detail: 'homepage'}));
  }

  /**
   * @param value The changed font size slider value.
   */
  private defaultFontSizeChanged_(value: number) {
    // This pref is handled separately in some extensions, but here it is tied
    // to default_font_size (to simplify the UI).
    this.set(
        'prefs.webkit.webprefs.default_fixed_font_size.value',
        value - SIZE_DIFFERENCE_FIXED_STANDARD);
  }

  /**
   * Open URL for either current theme or the theme gallery.
   */
  private openThemeUrl_() {
    window.open(this.themeUrl_ || loadTimeData.getString('themesGalleryUrl'));
  }

  private onUseDefaultClick_() {
    if (this.isForcedTheme_) {
      this.showManagedThemeDialog_ = true;
      return;
    }
    this.appearanceBrowserProxy_.useDefaultTheme();
  }

  // <if expr="is_linux">
  private systemThemePrefChanged_(systemTheme: SystemTheme) {
    this.systemTheme_ = systemTheme;
  }

  /** @return Whether to show the "USE CLASSIC" button. */
  private showUseClassic_(themeId: string): boolean {
    return !!themeId || this.systemTheme_ !== SystemTheme.DEFAULT;
  }

  /** @return Whether to show the "USE GTK" button. */
  private showUseGtk_(themeId: string): boolean {
    return (!!themeId || this.systemTheme_ !== SystemTheme.GTK) &&
        !this.appearanceBrowserProxy_.isChildAccount();
  }

  /** @return Whether to show the "USE QT" button. */
  private showUseQt_(themeId: string): boolean {
    return (!!themeId || this.systemTheme_ !== SystemTheme.QT) &&
        !this.appearanceBrowserProxy_.isChildAccount() &&
        loadTimeData.getBoolean('allowQtTheme');
  }

  /**
   * @return Whether to show the secondary area where "USE CLASSIC",
   *     "USE GTK", and "USE QT" buttons live.
   */
  private showThemesSecondary_(themeId: string): boolean {
    return !!themeId || !this.appearanceBrowserProxy_.isChildAccount();
  }

  private onUseGtkClick_() {
    if (this.isForcedTheme_) {
      this.showManagedThemeDialog_ = true;
      return;
    }
    this.appearanceBrowserProxy_.useGtkTheme();
  }

  private onUseQtClick_() {
    if (this.isForcedTheme_) {
      this.showManagedThemeDialog_ = true;
      return;
    }
    this.appearanceBrowserProxy_.useQtTheme();
  }
  // </if>

  private themeChanged_(themeId: string) {
    if (this.prefs === undefined || this.systemTheme_ === undefined) {
      return;
    }

    if (themeId.length > 0 && themeId !== AUTOGENERATED_THEME_ID &&
        !this.isForcedTheme_) {
      assert(this.systemTheme_ === SystemTheme.DEFAULT);

      this.appearanceBrowserProxy_.getThemeInfo(themeId).then(info => {
        this.themeSublabel_ = info.name;
      });

      this.themeUrl_ = 'https://chrome.google.com/webstore/detail/' + themeId;
      return;
    }

    this.themeUrl_ = '';

    if (themeId === AUTOGENERATED_THEME_ID || this.isForcedTheme_) {
      this.themeSublabel_ = this.i18n('chromeColors');
      return;
    }

    let i18nId;
    // <if expr="is_linux">
    switch (this.systemTheme_) {
      case SystemTheme.GTK:
        i18nId = 'gtkTheme';
        break;
      case SystemTheme.QT:
        i18nId = 'qtTheme';
        break;
      default:
        i18nId = 'classicTheme';
        break;
    }
    // </if>
    // <if expr="not is_linux">
    i18nId = 'chooseFromWebStore';
    // </if>
    this.themeSublabel_ = this.i18n(i18nId);
  }

  /** @return Whether applied theme is set by policy. */
  private computeIsForcedTheme_(): boolean {
    return !!this.getPref('autogenerated.theme.policy.color').controlledBy;
  }

  private onZoomLevelChange_() {
    chrome.settingsPrivate.setDefaultZoom(parseFloat(this.$.zoomLevel.value));
  }

  /** @see blink::PageZoomValuesEqual(). */
  private zoomValuesEqual_(zoom1: number, zoom2: number): boolean {
    return Math.abs(zoom1 - zoom2) <= 0.001;
  }

  private showHr_(previousIsVisible: boolean, nextIsVisible: boolean): boolean {
    return previousIsVisible && nextIsVisible;
  }

  private onManagedDialogClosed_() {
    this.showManagedThemeDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-appearance-page': SettingsAppearancePageElement;
  }
}

customElements.define(
    SettingsAppearancePageElement.is, SettingsAppearancePageElement);
