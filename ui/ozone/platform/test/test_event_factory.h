// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_TEST_TEST_EVENT_FACTORY_H_
#define UI_OZONE_PLATFORM_TEST_TEST_EVENT_FACTORY_H_

#include "ui/ozone/public/event_factory_ozone.h"

namespace ui {

class PlatformEventSource;

class TestEventFactory : public EventFactoryOzone {
 public:
  TestEventFactory();
  virtual ~TestEventFactory();

  // EventFactoryOzone:
  virtual void WarpCursorTo(gfx::AcceleratedWidget widget,
                            const gfx::PointF& location) OVERRIDE;

 private:
  scoped_ptr<PlatformEventSource> event_source_;

  DISALLOW_COPY_AND_ASSIGN(TestEventFactory);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_TEST_TEST_EVENT_FACTORY_H_
