// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.device_lock;

import android.view.View.OnClickListener;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

public class DeviceLockProperties {
    static final WritableBooleanPropertyKey PREEXISTING_DEVICE_LOCK =
            new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey DEVICE_SUPPORTS_PIN_CREATION_INTENT =
            new WritableBooleanPropertyKey();
    static final WritableBooleanPropertyKey IN_SIGN_IN_FLOW = new WritableBooleanPropertyKey();
    static final ReadableObjectPropertyKey<OnClickListener> ON_CREATE_DEVICE_LOCK_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> ON_GO_TO_OS_SETTINGS_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> ON_USER_UNDERSTANDS_CLICKED =
            new ReadableObjectPropertyKey<>();
    static final ReadableObjectPropertyKey<OnClickListener> ON_DISMISS_CLICKED =
            new ReadableObjectPropertyKey<>();

    static final PropertyKey[] ALL_KEYS = new PropertyKey[] {
            PREEXISTING_DEVICE_LOCK,
            DEVICE_SUPPORTS_PIN_CREATION_INTENT,
            IN_SIGN_IN_FLOW,
            ON_CREATE_DEVICE_LOCK_CLICKED,
            ON_GO_TO_OS_SETTINGS_CLICKED,
            ON_USER_UNDERSTANDS_CLICKED,
            ON_DISMISS_CLICKED,
    };

    private DeviceLockProperties() {}
}
