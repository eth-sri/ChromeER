// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "content/browser/accessibility/browser_accessibility_mac.h"

#import "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"

namespace content {

// Static.
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityMac();
}

BrowserAccessibilityMac::BrowserAccessibilityMac()
    : browser_accessibility_cocoa_(NULL) {
}

void BrowserAccessibilityMac::OnDataChanged() {
  BrowserAccessibility::OnDataChanged();

  if (browser_accessibility_cocoa_) {
    [browser_accessibility_cocoa_ childrenChanged];
    return;
  }

  // We take ownership of the cocoa obj here.
  browser_accessibility_cocoa_ = [[BrowserAccessibilityCocoa alloc]
      initWithObject:this];
}

void BrowserAccessibilityMac::NativeReleaseReference() {
  // Detach this object from |browser_accessibility_cocoa_| so it
  // no longer has a pointer to this object.
  [browser_accessibility_cocoa_ detach];
  // Now, release it - but at this point, other processes may have a
  // reference to the cocoa object.
  [browser_accessibility_cocoa_ release];
  // Finally, it's safe to delete this since we've detached.
  delete this;
}

bool BrowserAccessibilityMac::IsNative() const {
  return true;
}

BrowserAccessibilityCocoa* BrowserAccessibility::ToBrowserAccessibilityCocoa() {
  return static_cast<BrowserAccessibilityMac*>(this)->
      native_view();
}

}  // namespace content
