// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('INJECTOR: Loaded injector!');

chrome.tabs.create({ url: "test.html" });
