// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/surface_display_output_surface.h"

#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_manager.h"

namespace content {

SurfaceDisplayOutputSurface::SurfaceDisplayOutputSurface(
    cc::SurfaceManager* surface_manager,
    const scoped_refptr<cc::ContextProvider>& context_provider)
    : cc::OutputSurface(context_provider,
                        scoped_ptr<cc::SoftwareOutputDevice>()),
      display_(NULL),
      surface_manager_(surface_manager),
      factory_(surface_manager, this) {
  capabilities_.delegated_rendering = true;
  capabilities_.max_frames_pending = 1;
}

SurfaceDisplayOutputSurface::~SurfaceDisplayOutputSurface() {
}

void SurfaceDisplayOutputSurface::SwapBuffers(cc::CompositorFrame* frame) {
  gfx::Size frame_size =
      frame->delegated_frame_data->render_pass_list.back()->output_rect.size();
  display_->Resize(frame_size);
  cc::SurfaceId surface_id = display_->CurrentSurfaceId();
  if (surface_id.is_null())
    return;

  scoped_ptr<cc::CompositorFrame> frame_copy(new cc::CompositorFrame());
  frame->AssignTo(frame_copy.get());
  factory_.SubmitFrame(surface_id, frame_copy.Pass());

  if (!display_->Draw())
    return;

  client_->DidSwapBuffers();
  client_->DidSwapBuffersComplete();
}

void SurfaceDisplayOutputSurface::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  cc::CompositorFrameAck ack;
  ack.resources = resources;
  client_->ReclaimResources(&ack);
}

}  // namespace content
