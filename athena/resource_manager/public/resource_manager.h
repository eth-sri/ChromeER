// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_RESOURCE_MANAGER_PUBLIC_RESOURCE_MANAGER_H_
#define ATHENA_RESOURCE_MANAGER_PUBLIC_RESOURCE_MANAGER_H_

#include "athena/resource_manager/memory_pressure_notifier.h"
#include "base/basictypes.h"

namespace athena {

// The resource manager is monitoring activity changes, low memory conditions
// and other events to control the activity state (pre-/un-/re-/loading them)
// to keep enough memory free that no jank/lag will show when new applications
// are loaded and / or a navigation between applications takes place.
class ResourceManager {
 public:
  // Creates the instance handling the resources.
  static void Create();
  static ResourceManager* Get();
  static void Shutdown();

  ResourceManager();
  virtual ~ResourceManager();

  // Unit tests can simulate MemoryPressure changes with this call.
  // Note: Even though the default unit test ResourceManagerDelegte
  // implementation ensures that the MemoryPressure event will not go off,
  // this call will also explicitly stop the MemoryPressureNotifier.
  virtual void SetMemoryPressureAndStopMonitoring(
      MemoryPressureObserver::MemoryPressure pressure) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceManager);
};

}  // namespace athena

#endif  // ATHENA_RESOURCE_MANAGER_PUBLIC_RESOURCE_MANAGER_H_
