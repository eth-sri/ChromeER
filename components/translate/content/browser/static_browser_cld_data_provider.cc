// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "static_browser_cld_data_provider.h"

#include "content/public/browser/render_view_host.h"
#include "ipc/ipc_message.h"

namespace translate {

// Implementation of the static factory method from BrowserCldDataProvider,
// hooking up this specific implementation for all of Chromium.
BrowserCldDataProvider* CreateBrowserCldDataProviderFor(
    content::RenderViewHost* render_view_host) {
  return new StaticBrowserCldDataProvider();
}

StaticBrowserCldDataProvider::StaticBrowserCldDataProvider() {
}

StaticBrowserCldDataProvider::~StaticBrowserCldDataProvider() {
}

bool StaticBrowserCldDataProvider::OnMessageReceived(
    const IPC::Message& message) {
  // No-op: data is statically linked
  return false;
}

void StaticBrowserCldDataProvider::OnCldDataRequest() {
  // No-op: data is statically linked
}

void StaticBrowserCldDataProvider::SendCldDataResponse() {
  // No-op: data is statically linked
}

}  // namespace translate
