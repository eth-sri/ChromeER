// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/permission_infobar_delegate.h"

#include "chrome/browser/content_settings/permission_queue_controller.h"
#include "components/infobars/core/infobar.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace gcm {

PermissionInfobarDelegate::~PermissionInfobarDelegate() {
}

PermissionInfobarDelegate::PermissionInfobarDelegate(
    PermissionQueueController* controller,
    const PermissionRequestID& id,
    const GURL& requesting_frame)
    : controller_(controller), id_(id), requesting_frame_(requesting_frame) {
}

void PermissionInfobarDelegate::InfoBarDismissed() {
  SetPermission(false, false);
}

infobars::InfoBarDelegate::Type
PermissionInfobarDelegate::GetInfoBarType() const {
  return PAGE_ACTION_TYPE;
}

base::string16 PermissionInfobarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  return l10n_util::GetStringUTF16((button == BUTTON_OK) ?
      IDS_PERMISSION_ALLOW : IDS_PERMISSION_DENY);
}

bool PermissionInfobarDelegate::Accept() {
  SetPermission(true, true);
  return true;
}

bool PermissionInfobarDelegate::Cancel() {
  SetPermission(true, false);
  return true;
}

void PermissionInfobarDelegate::SetPermission(bool update_content_setting,
                                              bool allowed) {
  controller_->OnPermissionSet(
      id_, requesting_frame_,
      InfoBarService::WebContentsFromInfoBar(infobar())->GetURL(),
      update_content_setting, allowed);
}
}  // namespace gcm
