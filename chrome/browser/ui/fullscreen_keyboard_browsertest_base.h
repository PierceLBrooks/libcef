// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_FULLSCREEN_KEYBOARD_BROWSERTEST_BASE_H_
#define CHROME_BROWSER_UI_FULLSCREEN_KEYBOARD_BROWSERTEST_BASE_H_

#include <string>

#include "chrome/test/base/in_process_browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace content {
class WebContents;
}

class Browser;

class FullscreenKeyboardBrowserTestBase : public InProcessBrowserTest {
 public:
  FullscreenKeyboardBrowserTestBase();

  FullscreenKeyboardBrowserTestBase(const FullscreenKeyboardBrowserTestBase&) =
      delete;
  FullscreenKeyboardBrowserTestBase& operator=(
      const FullscreenKeyboardBrowserTestBase&) = delete;

  ~FullscreenKeyboardBrowserTestBase() override;

 protected:
  // InProcessBrowserTest override;
  void SetUpOnMainThread() override;

  // BrowserTestBase override
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Overridable to allow for custom test servers.
  virtual net::EmbeddedTestServer* GetEmbeddedTestServer();

  // Starts |kFullscreenKeyboardLockHTML| in a new tab and waits for load.
  void StartFullscreenLockPage();

  // Sends a control or command + |key| shortcut to the focused window. Shift
  // modifier will be added if |shift| is true.
  void SendShortcut(ui::KeyboardCode key, bool shift = false);

  // Sends a control or command + shift + |key| shortcut to the focused window.
  void SendShiftShortcut(ui::KeyboardCode key);

  // Sends a fullscreen shortcut to the focused window and wait for the
  // operation to take effect.
  void SendFullscreenShortcutAndWait();

  // Sends a KeyS to the focused window to trigger JavaScript fullscreen and
  // wait for the operation to take effect.
  void SendJsFullscreenShortcutAndWait();

  // Sends an ESC to the focused window.
  void SendEscape();

  // Sends an ESC to the focused window to exit JavaScript fullscreen and wait
  // for the operation to take effect.
  void SendEscapeAndWaitForExitingFullscreen();

  // Sends a set of preventable shortcuts to the web page and expects them to be
  // prevented.
  void SendShortcutsAndExpectPrevented();

  // Sends a set of preventable shortcuts to the web page and expects them to
  // not be prevented. If |js_fullscreen| is true, the test will use
  // SendJsFullscreenShortcutAndWait() to trigger the fullscreen mode. Otherwise
  // SendFullscreenShortcutAndWait() will be used.
  void SendShortcutsAndExpectNotPrevented(bool js_fullscreen);

  // Sends multiple shortcuts using the current window mode (i.e. fullscreen)
  // and verifies they have no effect on the current browser instance.
  void VerifyShortcutsAreNotPrevented();

  // Sends a magic KeyX to the focused window to stop the test case, receives
  // the result and verifies if it is equal to |expected_result_|.
  void FinishTestAndVerifyResult();

  // Returns whether the active tab is in html fullscreen mode.
  bool IsActiveTabFullscreen() const;

  // Returns whether the GetActiveBrowser() is in browser fullscreen mode.
  bool IsInBrowserFullscreen() const;

  content::WebContents* GetActiveWebContents() const;

  // Gets the current active tab index.
  int GetActiveTabIndex() const;

  // Gets the count of tabs in current browser.
  int GetTabCount() const;

  // Gets the count of browser instances.
  size_t GetBrowserCount() const;

  // Gets the last active Browser instance.
  Browser* GetActiveBrowser() const;

  // Creates a new browser instance.  Returns a pointer to the new instance.
  Browser* CreateNewBrowserInstance();

  // Ensures GetActiveBrowser() is focused.
  void FocusOnLastActiveBrowser();

  // Waits until the count of Browser instances becomes |expected|.
  void WaitForBrowserCount(size_t expected);

  // Waits until the count of the tabs in active Browser instance becomes
  // |expected|.
  void WaitForTabCount(int expected);

  // Waits until the index of active tab in active Browser instance becomes
  // |expected|.
  void WaitForActiveTabIndex(int expected);

  // Waits until the index of active tab in active Browser instance is not
  // |expected|.
  void WaitForInactiveTabIndex(int expected);

  // Returns the path for the fullscreen webpage used for testing.
  std::string GetFullscreenFramePath();

 private:
  // The expected output from the web page. This string is generated by
  // appending key presses from Send* functions above.
  std::string expected_result_;
};

#endif  // CHROME_BROWSER_UI_FULLSCREEN_KEYBOARD_BROWSERTEST_BASE_H_
