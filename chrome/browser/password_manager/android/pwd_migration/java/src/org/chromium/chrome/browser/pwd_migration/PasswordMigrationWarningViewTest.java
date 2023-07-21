// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pwd_migration;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.assertThat;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ACCOUNT_DISPLAY_NAME;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.VISIBLE;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.MigrationOption;
import org.chromium.chrome.browser.pwd_migration.PasswordMigrationWarningProperties.ScreenType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.DisableAnimationsTestRule;

/** Tests for {@link PasswordMigrationWarningView} */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PasswordMigrationWarningViewTest {
    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsRule =
            new DisableAnimationsTestRule();

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Mock
    private Callback<Integer> mDismissCallback;
    @Mock
    private PasswordMigrationWarningOnClickHandler mOnClickHandler;

    private BottomSheetController mBottomSheetController;
    private PasswordMigrationWarningView mView;
    private PropertyModel mModel;

    private static final String TEST_EMAIL = "user@domain.com";

    @Before
    public void setupTest() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.startMainActivityOnBlankPage();
        mBottomSheetController = mActivityTestRule.getActivity()
                                         .getRootUiCoordinatorForTesting()
                                         .getBottomSheetController();
        runOnUiThreadBlocking(() -> {
            mModel = PasswordMigrationWarningProperties.createDefaultModel(
                    mDismissCallback, mOnClickHandler);
            mView = new PasswordMigrationWarningView(
                    mActivityTestRule.getActivity(), mBottomSheetController);
            PropertyModelChangeProcessor.create(mModel, mView,
                    PasswordMigrationWarningViewBinder::bindPasswordMigrationWarningView);
        });
    }

    @Test
    @MediumTest
    public void testVisibilityChangedByModel() {
        // After setting the visibility to true, the view should exist and be visible.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        assertThat(mView.getContentView().isShown(), is(true));

        // After hiding the view, the view should still exist but be invisible.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);
        assertThat(mView.getContentView().isShown(), is(false));
    }

    @Test
    @MediumTest
    public void testDismissesWhenHidden() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // The sheet is hidden.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, false));
        pollUiThread(() -> getBottomSheetState() == BottomSheetController.SheetState.HIDDEN);

        // The dismiss callback was called.
        verify(mDismissCallback).onResult(BottomSheetController.StateChangeReason.NONE);
    }

    @Test
    @MediumTest
    public void testShowsIntroScreen() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // Setting the introduction screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.INTRO_SCREEN));
        // The test waits for the fragment containing the button to be attached.
        pollUiThread(()
                             -> mActivityTestRule.getActivity().findViewById(
                                        R.id.acknowledge_password_migration_button)
                        != null);
        onView(withId(R.id.migration_warning_sheet_subtitle)).check(matches(isDisplayed()));
        onView(withId(R.id.acknowledge_password_migration_button)).check(matches(isDisplayed()));
        onView(withId(R.id.password_migration_more_options_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testShowsOptionsScreen() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // Setting the options screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN));
        // The test waits for the fragment containing the button to be attached.
        pollUiThread(()
                             -> mActivityTestRule.getActivity().findViewById(
                                        R.id.password_migration_cancel_button)
                        != null);
        onView(withId(R.id.radio_button_layout)).check(matches(isDisplayed()));
        runOnUiThreadBlocking(() -> {
            RadioButtonWithDescription signInOrSyncButton =
                    mActivityTestRule.getActivity().findViewById(R.id.radio_sign_in_or_sync);
            assertTrue(signInOrSyncButton.isChecked());
        });
        onView(withId(R.id.password_migration_next_button)).check(matches(isDisplayed()));
        onView(withId(R.id.password_migration_cancel_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testNextButtonPropagatesSyncOption() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // Setting the options screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN));
        // The test waits for the fragment containing the button to be attached.
        pollUiThread(()
                             -> mActivityTestRule.getActivity().findViewById(
                                        R.id.password_migration_cancel_button)
                        != null);
        onView(withId(R.id.radio_button_layout)).check(matches(isDisplayed()));

        // Verify that the sync button is checked by default.
        runOnUiThreadBlocking(() -> {
            RadioButtonWithDescription signInOrSyncButton =
                    mActivityTestRule.getActivity().findViewById(R.id.radio_sign_in_or_sync);
            assertTrue(signInOrSyncButton.isChecked());
        });

        onView(withId(R.id.password_migration_next_button)).perform(click());
        verify(mOnClickHandler).onNext(MigrationOption.SYNC_PASSWORDS);
    }

    @Test
    @MediumTest
    public void testNextButtonPropagatesExportOption() {
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);
        // Setting the options screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN));
        // The test waits for the fragment containing the button to be attached.
        pollUiThread(()
                             -> mActivityTestRule.getActivity().findViewById(
                                        R.id.password_migration_cancel_button)
                        != null);
        onView(withId(R.id.radio_button_layout)).check(matches(isDisplayed()));

        // Select the export button.
        runOnUiThreadBlocking(() -> {
            RadioButtonWithDescription exportButton =
                    mActivityTestRule.getActivity().findViewById(R.id.radio_password_export);
            exportButton.setChecked(true);
        });

        onView(withId(R.id.password_migration_next_button)).perform(click());
        verify(mOnClickHandler).onNext(MigrationOption.EXPORT_AND_DELETE);
    }

    /**
     * Checks that no crash happens and everything works as expected if CURRENT_SCREEN will be set
     * first. It can happen in production, because the order is not guaranteed.
     */
    @Test
    @MediumTest
    public void testCurrentScreenChangedBeforeVisibility() {
        // Setting the introduction screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.INTRO_SCREEN));
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        pollUiThread(()
                             -> mActivityTestRule.getActivity().findViewById(
                                        R.id.acknowledge_password_migration_button)
                        != null);
        onView(withId(R.id.migration_warning_sheet_subtitle)).check(matches(isDisplayed()));
        onView(withId(R.id.acknowledge_password_migration_button)).check(matches(isDisplayed()));
        onView(withId(R.id.password_migration_more_options_button)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAccountNameIsSet() {
        // Setting the options screen.
        runOnUiThreadBlocking(() -> mModel.set(CURRENT_SCREEN, ScreenType.OPTIONS_SCREEN));
        // Setting the profile.
        runOnUiThreadBlocking(() -> mModel.set(ACCOUNT_DISPLAY_NAME, TEST_EMAIL));
        // The sheet is shown.
        runOnUiThreadBlocking(() -> mModel.set(VISIBLE, true));
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        pollUiThread(()
                             -> mActivityTestRule.getActivity().findViewById(
                                        R.id.password_migration_next_button)
                        != null);
        onView(withText(TEST_EMAIL)).check(matches(isDisplayed()));
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }
}
