// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Background page runs the test after we've loaded.
chrome.extension.getBackgroundPage().runTest(window);
