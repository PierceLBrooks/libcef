// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {AppearanceBrowserProxy, AppearanceBrowserProxyImpl,HomeUrlInputElement, SettingsAppearancePageElement, SystemTheme} from 'chrome://settings/settings.js';
import {assertEquals,assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';
// clang-format on

class TestAppearanceBrowserProxy extends TestBrowserProxy implements
    AppearanceBrowserProxy {
  private defaultZoom_: number = 1;
  private isChildAccount_: boolean = false;
  private isHomeUrlValid_: boolean = true;

  constructor() {
    super([
      'getDefaultZoom',
      'getThemeInfo',
      'isChildAccount',
      'useDefaultTheme',
      // <if expr="is_linux">
      'useGtkTheme',
      'useQtTheme',
      // </if>
      'validateStartupPage',
    ]);
  }

  getDefaultZoom() {
    this.methodCalled('getDefaultZoom');
    return Promise.resolve(this.defaultZoom_);
  }

  getThemeInfo(themeId: string) {
    this.methodCalled('getThemeInfo', themeId);
    return Promise.resolve({
      id: '',
      name: 'Sports car red',
      shortName: '',
      description: '',
      version: '',
      mayDisable: false,
      enabled: false,
      isApp: false,
      offlineEnabled: false,
      optionsUrl: '',
      permissions: [],
      hostPermissions: [],
    });
  }

  isChildAccount() {
    this.methodCalled('isChildAccount');
    return this.isChildAccount_;
  }

  useDefaultTheme() {
    this.methodCalled('useDefaultTheme');
  }

  // <if expr="is_linux">
  useGtkTheme() {
    this.methodCalled('useGtkTheme');
  }

  useQtTheme() {
    this.methodCalled('useQtTheme');
  }
  // </if>

  setDefaultZoom(defaultZoom: number) {
    this.defaultZoom_ = defaultZoom;
  }

  setIsChildAccount(isChildAccount: boolean) {
    this.isChildAccount_ = isChildAccount;
  }

  validateStartupPage(url: string) {
    this.methodCalled('validateStartupPage', url);
    return Promise.resolve(this.isHomeUrlValid_);
  }

  setValidStartupPageResponse(isValid: boolean) {
    this.isHomeUrlValid_ = isValid;
  }
}

let appearancePage: SettingsAppearancePageElement;
let appearanceBrowserProxy: TestAppearanceBrowserProxy;

function createAppearancePage() {
  appearanceBrowserProxy.reset();
  document.body.innerHTML = window.trustedTypes!.emptyHTML;

  appearancePage = document.createElement('settings-appearance-page');
  appearancePage.set('prefs', {
    autogenerated: {
      theme: {
        policy: {
          color: {
            value: 0,
          },
        },
      },
    },
    extensions: {
      theme: {
        id: {
          value: '',
        },
        system_theme: {
          value: SystemTheme.DEFAULT,
        },
      },
    },
  });

  appearancePage.set('pageVisibility', {
    setWallpaper: true,
  });

  document.body.appendChild(appearancePage);
  flush();
}

suite('AppearanceHandler', function() {
  setup(function() {
    appearanceBrowserProxy = new TestAppearanceBrowserProxy();
    AppearanceBrowserProxyImpl.setInstance(appearanceBrowserProxy);
    createAppearancePage();
  });

  teardown(function() {
    appearancePage.remove();
  });

  const THEME_ID_PREF = 'prefs.extensions.theme.id.value';

  // <if expr="is_linux">
  const SYSTEM_THEME_PREF = 'prefs.extensions.theme.system_theme.value';

  test('useDefaultThemeLinux', function() {
    assertFalse(!!appearancePage.get(THEME_ID_PREF));
    assertEquals(appearancePage.get(SYSTEM_THEME_PREF), SystemTheme.DEFAULT);
    // No custom nor system theme in use; "USE CLASSIC" should be hidden.
    assertFalse(!!appearancePage.shadowRoot!.querySelector('#useDefault'));

    appearancePage.set(SYSTEM_THEME_PREF, SystemTheme.GTK);
    flush();
    // If the system theme is in use, "USE CLASSIC" should show.
    assertTrue(!!appearancePage.shadowRoot!.querySelector('#useDefault'));

    appearancePage.set(SYSTEM_THEME_PREF, SystemTheme.DEFAULT);
    appearancePage.set(THEME_ID_PREF, 'fake theme id');
    flush();

    // With a custom theme installed, "USE CLASSIC" should show.
    const button =
        appearancePage.shadowRoot!.querySelector<HTMLElement>('#useDefault');
    assertTrue(!!button);

    button!.click();
    return appearanceBrowserProxy.whenCalled('useDefaultTheme');
  });

  test('useGtkThemeLinux', function() {
    assertFalse(!!appearancePage.get(THEME_ID_PREF));
    appearancePage.set(SYSTEM_THEME_PREF, SystemTheme.GTK);
    flush();
    // The "USE GTK+" button shouldn't be showing if it's already in use.
    assertFalse(!!appearancePage.shadowRoot!.querySelector('#useGtk'));

    appearanceBrowserProxy.setIsChildAccount(true);
    appearancePage.set(SYSTEM_THEME_PREF, SystemTheme.DEFAULT);
    flush();
    // Child account users have their own theme and can't use GTK+ theme.
    assertFalse(!!appearancePage.shadowRoot!.querySelector('#useDefault'));
    assertFalse(!!appearancePage.shadowRoot!.querySelector('#useGtk'));
    // If there's no "USE" buttons, the container should be hidden.
    assertTrue(
        appearancePage.shadowRoot!
            .querySelector<HTMLElement>('#themesSecondaryActions')!.hidden);

    appearanceBrowserProxy.setIsChildAccount(false);
    appearancePage.set(THEME_ID_PREF, 'fake theme id');
    flush();
    // If there's "USE" buttons again, the container should be visible.
    assertTrue(!!appearancePage.shadowRoot!.querySelector('#useDefault'));
    assertFalse(
        appearancePage.shadowRoot!
            .querySelector<HTMLElement>('#themesSecondaryActions')!.hidden);

    const button =
        appearancePage.shadowRoot!.querySelector<HTMLElement>('#useGtk');
    assertTrue(!!button);

    button!.click();
    return appearanceBrowserProxy.whenCalled('useGtkTheme');
  });
  // </if>

  // <if expr="not is_linux">
  test('useDefaultTheme', function() {
    assertFalse(!!appearancePage.get(THEME_ID_PREF));
    assertFalse(!!appearancePage.shadowRoot!.querySelector('#useDefault'));

    appearancePage.set(THEME_ID_PREF, 'fake theme id');
    flush();

    // With a custom theme installed, "RESET TO DEFAULT" should show.
    const button =
        appearancePage.shadowRoot!.querySelector<HTMLElement>('#useDefault');
    assertTrue(!!button);

    button!.click();
    return appearanceBrowserProxy.whenCalled('useDefaultTheme');
  });

  test('useDefaultThemeWithPolicy', function() {
    const POLICY_THEME_COLOR_PREF = 'prefs.autogenerated.theme.policy.color';
    assertFalse(!!appearancePage.shadowRoot!.querySelector('#useDefault'));

    // "Reset to default" button doesn't appear as result of a policy theme.
    appearancePage.set(POLICY_THEME_COLOR_PREF, {controlledBy: 'PRIMARY_USER'});
    flush();

    assertFalse(!!appearancePage.shadowRoot!.querySelector('#useDefault'));

    // Unset policy theme and set custom theme to get button to show.
    appearancePage.set(POLICY_THEME_COLOR_PREF, {});
    appearancePage.set(THEME_ID_PREF, 'fake theme id');
    flush();

    let button =
        appearancePage.shadowRoot!.querySelector<HTMLElement>('#useDefault');
    assertTrue(!!button);

    // Clicking "Reset to default" button when a policy theme is applied
    // causes the managed theme dialog to appear.
    appearancePage.set(POLICY_THEME_COLOR_PREF, {controlledBy: 'PRIMARY_USER'});
    flush();

    button =
        appearancePage.shadowRoot!.querySelector<HTMLElement>('#useDefault');
    assertTrue(!!button);
    assertEquals(
        null, appearancePage.shadowRoot!.querySelector('managed-dialog'));

    button!.click();
    flush();

    assertFalse(
        appearancePage.shadowRoot!.querySelector('managed-dialog')!.hidden);
  });
  // </if>

  test('default zoom handling', async function() {
    function getDefaultZoomText() {
      const zoomLevel = appearancePage.$.zoomLevel;
      return zoomLevel.options[zoomLevel.selectedIndex]!.textContent!.trim();
    }

    await appearanceBrowserProxy.whenCalled('getDefaultZoom');

    assertEquals('100%', getDefaultZoomText());

    appearanceBrowserProxy.setDefaultZoom(2 / 3);
    createAppearancePage();
    await appearanceBrowserProxy.whenCalled('getDefaultZoom');

    assertEquals('67%', getDefaultZoomText());

    appearanceBrowserProxy.setDefaultZoom(11 / 10);
    createAppearancePage();
    await appearanceBrowserProxy.whenCalled('getDefaultZoom');

    assertEquals('110%', getDefaultZoomText());

    appearanceBrowserProxy.setDefaultZoom(1.7499999999999);
    createAppearancePage();
    await appearanceBrowserProxy.whenCalled('getDefaultZoom');

    assertEquals('175%', getDefaultZoomText());
  });

  test('show home button toggling', function() {
    assertFalse(
        !!appearancePage.shadowRoot!.querySelector('#home-button-options'));
    appearancePage.set('prefs', {
      autogenerated: {theme: {policy: {color: {value: 0}}}},
      browser: {show_home_button: {value: true}},
      extensions: {theme: {id: {value: ''}}},
    });
    flush();

    assertTrue(
        !!appearancePage.shadowRoot!.querySelector('#home-button-options'));
  });

  test('show side panel options', function() {
    loadTimeData.overrideValues({
      showSidePanelOptions: true,
    });
    createAppearancePage();
    assertTrue(!!appearancePage.shadowRoot!.querySelector('#side-panel'));

    loadTimeData.overrideValues({
      showSidePanelOptions: false,
    });
    createAppearancePage();
    assertFalse(!!appearancePage.shadowRoot!.querySelector('#side-panel'));
  });

});

suite('HomeUrlInput', function() {
  let homeUrlInput: HomeUrlInputElement;

  setup(function() {
    appearanceBrowserProxy = new TestAppearanceBrowserProxy();
    AppearanceBrowserProxyImpl.setInstance(appearanceBrowserProxy);
    document.body.innerHTML = window.trustedTypes!.emptyHTML;

    homeUrlInput = document.createElement('home-url-input');
    homeUrlInput.set(
        'pref', {type: chrome.settingsPrivate.PrefType.URL, value: 'test'});

    document.body.appendChild(homeUrlInput);
    flush();
  });

  test('home button urls', async function() {
    assertFalse(homeUrlInput.invalid);
    assertEquals(homeUrlInput.value, 'test');

    homeUrlInput.value = '@@@';
    appearanceBrowserProxy.setValidStartupPageResponse(false);
    homeUrlInput.$.input.dispatchEvent(
        new CustomEvent('input', {bubbles: true, composed: true}));

    const url = await appearanceBrowserProxy.whenCalled('validateStartupPage');

    assertEquals(homeUrlInput.value, url);
    flush();
    assertEquals(homeUrlInput.value, '@@@');  // Value hasn't changed.
    assertTrue(homeUrlInput.invalid);

    // Should reset to default value on change event.
    homeUrlInput.$.input.dispatchEvent(
        new CustomEvent('change', {bubbles: true, composed: true}));
    flush();
    assertEquals(homeUrlInput.value, 'test');
  });
});
