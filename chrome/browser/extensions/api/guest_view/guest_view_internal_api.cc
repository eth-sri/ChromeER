// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/guest_view/guest_view_internal_api.h"

#include "chrome/browser/guest_view/guest_view_base.h"
#include "chrome/browser/guest_view/guest_view_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/guest_view_internal.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "extensions/common/permissions/permissions_data.h"

namespace guest_view_internal = extensions::api::guest_view_internal;

namespace extensions {

GuestViewInternalCreateGuestFunction::
    GuestViewInternalCreateGuestFunction() {
}

bool GuestViewInternalCreateGuestFunction::RunAsync() {
  std::string view_type;
  EXTENSION_FUNCTION_VALIDATE(args_->GetString(0, &view_type));

  base::DictionaryValue* create_params;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(1, &create_params));

  GuestViewManager* guest_view_manager =
      GuestViewManager::FromBrowserContext(browser_context());

  GuestViewManager::WebContentsCreatedCallback callback =
      base::Bind(&GuestViewInternalCreateGuestFunction::CreateGuestCallback,
                 this);
  guest_view_manager->CreateGuest(view_type,
                                  extension_id(),
                                  render_view_host()->GetProcess()->GetID(),
                                  *create_params,
                                  callback);

  return true;
}

void GuestViewInternalCreateGuestFunction::CreateGuestCallback(
    content::WebContents* guest_web_contents) {
  int guest_instance_id = 0;
  if (guest_web_contents) {
    GuestViewBase* guest = GuestViewBase::FromWebContents(guest_web_contents);
    guest_instance_id = guest->GetGuestInstanceID();
  }
  SetResult(new base::FundamentalValue(guest_instance_id));
  SendResponse(true);
}

GuestViewInternalSetAutoSizeFunction::
    GuestViewInternalSetAutoSizeFunction() {
}

GuestViewInternalSetAutoSizeFunction::
    ~GuestViewInternalSetAutoSizeFunction() {
}

bool GuestViewInternalSetAutoSizeFunction::RunAsync() {
  scoped_ptr<guest_view_internal::SetAutoSize::Params> params(
      guest_view_internal::SetAutoSize::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  GuestViewBase* guest = GuestViewBase::From(
      render_view_host()->GetProcess()->GetID(), params->instance_id);
  if (!guest)
    return false;
  guest->SetAutoSize(params->params.enable_auto_size,
                     gfx::Size(params->params.min.width,
                               params->params.min.height),
                     gfx::Size(params->params.max.width,
                               params->params.max.height));
  SendResponse(true);
  return true;
}

}  // namespace extensions
