// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "extensions/browser/event_router.h"
#include "extensions/common/api/test.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace OnMessage = core_api::test::OnMessage;

namespace {

void FindFrame(const GURL& url,
               content::RenderFrameHost** out,
               content::RenderFrameHost* frame) {
  if (frame->GetLastCommittedURL() == url) {
    if (*out != NULL) {
      ADD_FAILURE() << "Found multiple frames at " << url;
    }
    *out = frame;
  }
}

// Tests running extension APIs on WebUI.
class ExtensionWebUITest : public ExtensionApiTest {
 protected:
  testing::AssertionResult RunTest(const char* name,
                                   const GURL& page_url,
                                   const GURL& frame_url,
                                   bool expected_result) {
    // Tests are located in chrome/test/data/extensions/webui/$(name).
    base::FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path =
        path.AppendASCII("extensions").AppendASCII("webui").AppendASCII(name);

    // Read the test.
    if (!base::PathExists(path))
      return testing::AssertionFailure() << "Couldn't find " << path.value();
    std::string script;
    base::ReadFileToString(path, &script);
    script = "(function(){'use strict';" + script + "}());";

    // Run the test.
    bool actual_result = false;
    content::RenderFrameHost* webui = NavigateToWebUI(page_url, frame_url);
    if (!webui)
      return testing::AssertionFailure() << "Failed to navigate to WebUI";
    CHECK(content::ExecuteScriptAndExtractBool(webui, script, &actual_result));
    return (expected_result == actual_result)
               ? testing::AssertionSuccess()
               : (testing::AssertionFailure() << "Check console output");
  }

  testing::AssertionResult RunTestOnExtensions(const char* name) {
    // In the current design the URL of the chrome://extensions page it's
    // actually chrome://extensions-frame/ -- and it's important we find it,
    // because the top-level frame doesn't execute any code, so a script
    // context is never created, so the bindings are never set up, and
    // apparently the call to ExecuteScriptAndExtractString doesn't adequately
    // set them up either.
    return RunTest(name,
                   GURL("chrome://extensions"),
                   GURL("chrome://extensions-frame"),
                   true);  // tests on chrome://extensions should succeed
  }

  testing::AssertionResult RunTestOnAbout(const char* name) {
    // chrome://about is an innocuous page that doesn't have any bindings.
    // Tests should fail.
    return RunTest(name,
                   GURL("chrome://about"),
                   GURL("chrome://about"),
                   false);  // tests on chrome://about should fail
  }

 private:
  // Navigates the browser to a WebUI page and returns the RenderFrameHost for
  // that page.
  content::RenderFrameHost* NavigateToWebUI(const GURL& page_url,
                                            const GURL& frame_url) {
    ui_test_utils::NavigateToURL(browser(), page_url);

    content::WebContents* active_web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    if (active_web_contents->GetLastCommittedURL() == frame_url)
      return active_web_contents->GetMainFrame();

    content::RenderFrameHost* frame_host = NULL;
    active_web_contents->ForEachFrame(
        base::Bind(&FindFrame, frame_url, &frame_host));
    return frame_host;
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, SanityCheckAvailableAPIs) {
  ASSERT_TRUE(RunTestOnExtensions("sanity_check_available_apis.js"));
}

IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, SanityCheckUnavailableAPIs) {
  ASSERT_TRUE(RunTestOnAbout("sanity_check_available_apis.js"));
}

// Tests chrome.test.sendMessage, which exercises WebUI making a
// function call and receiving a response.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, SendMessage) {
  scoped_ptr<ExtensionTestMessageListener> listener(
      new ExtensionTestMessageListener("ping", true));

  ASSERT_TRUE(RunTestOnExtensions("send_message.js"));

  ASSERT_TRUE(listener->WaitUntilSatisfied());
  listener->Reply("pong");

  listener.reset(new ExtensionTestMessageListener(false));
  ASSERT_TRUE(listener->WaitUntilSatisfied());
  EXPECT_EQ("true", listener->message());
}

// Tests chrome.runtime.onMessage, which exercises WebUI registering and
// receiving an event.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, OnMessage) {
  ASSERT_TRUE(RunTestOnExtensions("on_message.js"));

  OnMessage::Info info;
  info.data = "hi";
  info.last_message = true;
  EventRouter::Get(profile())->BroadcastEvent(make_scoped_ptr(
      new Event(OnMessage::kEventName, OnMessage::Create(info))));

  scoped_ptr<ExtensionTestMessageListener> listener(
      new ExtensionTestMessageListener(false));
  ASSERT_TRUE(listener->WaitUntilSatisfied());
  EXPECT_EQ("true", listener->message());
}

// Tests chrome.runtime.lastError, which exercises WebUI accessing a property
// on an API which it doesn't actually have access to. A bindings test really.
IN_PROC_BROWSER_TEST_F(ExtensionWebUITest, RuntimeLastError) {
  scoped_ptr<ExtensionTestMessageListener> listener(
      new ExtensionTestMessageListener("ping", true));

  ASSERT_TRUE(RunTestOnExtensions("runtime_last_error.js"));

  ASSERT_TRUE(listener->WaitUntilSatisfied());
  listener->ReplyWithError("unknown host");

  listener.reset(new ExtensionTestMessageListener(false));
  ASSERT_TRUE(listener->WaitUntilSatisfied());
  EXPECT_EQ("true", listener->message());
}

}  // namespace

}  // namespace extensions
