// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ACCOUNT_DISPLAY_NAME;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningMediator.MigrationWarningOptionsHandler;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.MigrationOption;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.AccountCapabilitiesConstants;
import org.chromium.components.signin.base.AccountCapabilities;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountId;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 * Tests for {@link PasswordMigrationWarningMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.PER_CLASS)
public class PasswordMigrationWarningMediatorTest {
    private static final String TEST_EMAIL = "user@domain.com";
    private static final String FULL_NAME = "full name";
    private static final AccountInfo ACCOUNT_INFO =
            new AccountInfo(new CoreAccountId("gaia-id-user"), TEST_EMAIL, "gaia-id-user",
                    FULL_NAME, "given name", null, new AccountCapabilities(new HashMap<>()));
    private static final AccountInfo NON_DISPLAYABLE_EMAIL_ACCOUNT_INFO = new AccountInfo(
            new CoreAccountId("gaia-id-user"), TEST_EMAIL, "gaia-id-user", FULL_NAME, "given name",
            null,
            new AccountCapabilities(new HashMap<>(Map.of(
                    AccountCapabilitiesConstants.CAN_HAVE_EMAIL_ADDRESS_DISPLAYED_CAPABILITY_NAME,
                    false))));

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Rule
    public JniMocker mJniMocker = new JniMocker();

    private PasswordMigrationWarningMediator mMediator;
    private PropertyModel mModel;

    @Mock
    private BottomSheetController mBottomSheetController;
    @Mock
    private Profile mProfile;
    @Mock
    private MigrationWarningOptionsHandler mOptionsHandler;
    @Mock
    private UserPrefs.Natives mUserPrefsJni;
    @Mock
    private PrefService mPrefService;
    @Mock
    private IdentityServicesProvider mIdentityServicesProvider;
    @Mock
    private IdentityManager mIdentityManager;
    @Mock
    private SyncService mSyncService;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(UserPrefsJni.TEST_HOOKS, mUserPrefsJni);
        mMediator = new PasswordMigrationWarningMediator(mProfile, mOptionsHandler);
        mModel = PasswordMigrationWarningProperties.createDefaultModel(
                mMediator::onDismissed, mMediator);
        mMediator.initialize(mModel);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProvider);
        SyncServiceFactory.overrideForTests(mSyncService);
    }

    @Test
    public void testShowWarningChangesVisibility() {
        mModel.set(VISIBLE, false);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertTrue(mModel.get(VISIBLE));
    }

    @Test
    public void testOnDismissedHidesTheSheet() {
        mModel.set(VISIBLE, true);
        mMediator.onDismissed(StateChangeReason.NONE);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testDismissHandlerHidesTheSheet() {
        assertNotNull(mModel.get(DISMISS_HANDLER));
        mModel.set(VISIBLE, true);
        mModel.get(DISMISS_HANDLER).onResult(StateChangeReason.NONE);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOnMoreOptionsChangesTheModel() {
        mModel.set(VISIBLE, true);
        mModel.set(CURRENT_SCREEN, ScreenType.INTRO_SCREEN);
        mMediator.onMoreOptions();
        assertEquals(mModel.get(CURRENT_SCREEN), ScreenType.OPTIONS_SCREEN);
    }

    @Test
    public void testOnAcknowledgeHidesTheSheet() {
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        mModel.set(VISIBLE, true);
        mMediator.onAcknowledge(mBottomSheetController);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testAcknowledgementIsSavedInPrefs() {
        when(mUserPrefsJni.get(mProfile)).thenReturn(mPrefService);
        mMediator.onAcknowledge(mBottomSheetController);
        verify(mPrefService)
                .setBoolean(eq(Pref.USER_ACKNOWLEDGED_LOCAL_PASSWORDS_MIGRATION_WARNING), eq(true));
    }

    @Test
    public void testOnCancelHidesTheSheet() {
        mModel.set(VISIBLE, true);
        mMediator.onCancel(mBottomSheetController);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOnNextDismissesTheSheet() {
        mModel.set(VISIBLE, true);
        mMediator.onNext(MigrationOption.SYNC_PASSWORDS);
        assertFalse(mModel.get(VISIBLE));
    }

    @Test
    public void testOnNextSelectedSyncNoConsent() {
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(false);
        mMediator.onNext(MigrationOption.SYNC_PASSWORDS);
        verify(mOptionsHandler).startSyncConsentFlow();
    }

    @Test
    public void testOnNextSelectedSyncWithConsentButPasswordsDisabled() {
        when(mSyncService.isSyncFeatureEnabled()).thenReturn(true);
        when(mSyncService.getSelectedTypes()).thenReturn(Collections.EMPTY_SET);
        mMediator.onNext(MigrationOption.SYNC_PASSWORDS);
        verify(mOptionsHandler).openSyncSettings();
    }

    @Test
    public void testGetAccountDisplayNameReturnsEmail() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL, "0"));
        when(mIdentityManager.findExtendedAccountInfoByEmailAddress(TEST_EMAIL))
                .thenReturn(ACCOUNT_INFO);

        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertEquals(TEST_EMAIL, mModel.get(ACCOUNT_DISPLAY_NAME));
    }

    @Test
    public void testGetAccountDisplayNameReturnsFullName() {
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(CoreAccountInfo.createFromEmailAndGaiaId(TEST_EMAIL, "0"));
        when(mIdentityManager.findExtendedAccountInfoByEmailAddress(TEST_EMAIL))
                .thenReturn(NON_DISPLAYABLE_EMAIL_ACCOUNT_INFO);

        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertEquals(FULL_NAME, mModel.get(ACCOUNT_DISPLAY_NAME));
    }

    @Test
    public void testGetAccountDisplayNameWithNoSignedInUser() {
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN)).thenReturn(null);
        when(mIdentityServicesProvider.getIdentityManager(mProfile)).thenReturn(mIdentityManager);

        mMediator.showWarning(ScreenType.INTRO_SCREEN);
        assertEquals(null, mModel.get(ACCOUNT_DISPLAY_NAME));
    }
}
