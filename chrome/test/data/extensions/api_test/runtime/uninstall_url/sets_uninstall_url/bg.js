// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.setUninstallURL("http://www.google.com");

chrome.test.sendMessage('ready');
