// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_PRIVATE_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_PRIVATE_MAC_H_

#include "content/browser/compositor/browser_compositor_view_mac.h"

namespace content {
class BrowserCompositorViewCocoaHelper;
}

// An NSView drawn by a ui::Compositor. This structure is expensive to create,
// because it has a ui::Compositor. As a result, this structure may be recycled
// across multiple BrowserCompositorViewMac objects.
@interface BrowserCompositorViewCocoa : NSView {
  scoped_ptr<ui::Compositor> compositor_;

  base::scoped_nsobject<CALayer> background_layer_;
  base::scoped_nsobject<CompositingIOSurfaceLayer> accelerated_layer_;
  int accelerated_layer_output_surface_id_;
  std::vector<ui::LatencyInfo> accelerated_latency_info_;
  base::scoped_nsobject<SoftwareLayer> software_layer_;

  content::BrowserCompositorViewMacClient* client_;
  scoped_ptr<content::BrowserCompositorViewCocoaHelper> helper_;
}

// Change the client and superview of the view.
- (void)setClient:(content::BrowserCompositorViewMacClient*)client;

// Access the underlying ui::Compositor for this view.
- (ui::Compositor*)compositor;

// Called when the accelerated or software layer draws its frame to the screen.
- (void)layerDidDrawFrame;

// Called when an error is encountered while drawing to the screen.
- (void)gotAcceleratedLayerError;

@end  // BrowserCompositorViewCocoa

namespace content {

// This class implements the parts of BrowserCompositorViewCocoa that need to
// be a C++ class and not an Objective C class.
class BrowserCompositorViewCocoaHelper
    : public content::CompositingIOSurfaceLayerClient {
 public:
  BrowserCompositorViewCocoaHelper(BrowserCompositorViewCocoa* view)
      : view_(view) {}
  virtual ~BrowserCompositorViewCocoaHelper() {}

 private:
  // CompositingIOSurfaceLayerClient implementation:
  virtual void AcceleratedLayerDidDrawFrame(bool succeeded) OVERRIDE;

  BrowserCompositorViewCocoa* view_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_BROWSER_COMPOSITOR_VIEW_PRIVATE_MAC_H_
