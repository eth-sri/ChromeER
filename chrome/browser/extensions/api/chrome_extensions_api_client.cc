// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"

#include "base/files/file_path.h"
#include "chrome/browser/extensions/api/storage/sync_value_store_cache.h"
#include "chrome/browser/guest_view/app_view/chrome_app_view_guest_delegate.h"
#include "chrome/browser/guest_view/extension_options/extension_options_guest.h"
#include "chrome/browser/guest_view/web_view/chrome_web_view_guest_delegate.h"
#include "chrome/browser/guest_view/web_view/chrome_web_view_permission_helper_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "device/hid/hid_service.h"
#include "extensions/browser/guest_view/app_view/app_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/guest_view/web_view/web_view_permission_helper.h"

#if defined(ENABLE_CONFIGURATION_POLICY)
#include "chrome/browser/extensions/api/storage/managed_value_store_cache.h"
#endif

namespace extensions {

ChromeExtensionsAPIClient::ChromeExtensionsAPIClient() {}

ChromeExtensionsAPIClient::~ChromeExtensionsAPIClient() {}

void ChromeExtensionsAPIClient::AddAdditionalValueStoreCaches(
    content::BrowserContext* context,
    const scoped_refptr<SettingsStorageFactory>& factory,
    const scoped_refptr<ObserverListThreadSafe<SettingsObserver> >& observers,
    std::map<settings_namespace::Namespace, ValueStoreCache*>* caches) {
  // Add support for chrome.storage.sync.
  (*caches)[settings_namespace::SYNC] =
      new SyncValueStoreCache(factory, observers, context->GetPath());

#if defined(ENABLE_CONFIGURATION_POLICY)
  // Add support for chrome.storage.managed.
  (*caches)[settings_namespace::MANAGED] =
      new ManagedValueStoreCache(context, factory, observers);
#endif
}

AppViewGuestDelegate* ChromeExtensionsAPIClient::CreateAppViewGuestDelegate()
    const {
  return new ChromeAppViewGuestDelegate();
}

WebViewGuestDelegate* ChromeExtensionsAPIClient::CreateWebViewGuestDelegate(
    WebViewGuest* web_view_guest) const {
  return new ChromeWebViewGuestDelegate(web_view_guest);
}

WebViewPermissionHelperDelegate* ChromeExtensionsAPIClient::
    CreateWebViewPermissionHelperDelegate(
        WebViewPermissionHelper* web_view_permission_helper) const {
  return new ChromeWebViewPermissionHelperDelegate(web_view_permission_helper);
}

device::HidService* ChromeExtensionsAPIClient::GetHidService() {
  if (!hid_service_) {
    hid_service_.reset(device::HidService::Create(
        content::BrowserThread::GetMessageLoopProxyForThread(
            content::BrowserThread::UI)));
  }
  return hid_service_.get();
}

void ChromeExtensionsAPIClient::RegisterGuestViewTypes() {
  ExtensionOptionsGuest::Register();
}

}  // namespace extensions
