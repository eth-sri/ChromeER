// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_INFOBAR_DELEGATE_H_
#define CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_INFOBAR_DELEGATE_H_

#include "chrome/browser/services/gcm/permission_infobar_delegate.h"

class GURL;
class InfoBarService;

namespace gcm {

// Delegate to allow GCM push messages registration.
class PushMessagingInfoBarDelegate : public PermissionInfobarDelegate {
 public:
  virtual ~PushMessagingInfoBarDelegate();

  // Creates a Push Permission infobar and delegate and adds the infobar to
  // |infobar_service|.  Returns the infobar if it was successfully added.
  static infobars::InfoBar* Create(InfoBarService* infobar_service,
                                   PermissionQueueController* controller,
                                   const PermissionRequestID& id,
                                   const GURL& requesting_frame,
                                   const std::string& display_languages);

 private:
  PushMessagingInfoBarDelegate(PermissionQueueController* controller,
                               const PermissionRequestID& id,
                               const GURL& requesting_frame,
                               const std::string& display_languages);

  // ConfirmInfoBarDelegate:
  virtual base::string16 GetMessageText() const OVERRIDE;

  const GURL requesting_origin_;
  const std::string display_languages_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingInfoBarDelegate);
};

}  // namespace gcm
#endif  // CHROME_BROWSER_SERVICES_GCM_PUSH_MESSAGING_INFOBAR_DELEGATE_H_

