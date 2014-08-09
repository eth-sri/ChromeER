// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_OUT_OF_PROCESS_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_OUT_OF_PROCESS_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "chrome/browser/component_updater/component_patcher_operation.h"

namespace component_updater {

class PatchHost;

// Implements the DeltaUpdateOpPatch out-of-process patching.
class ChromeOutOfProcessPatcher : public OutOfProcessPatcher {
 public:
  ChromeOutOfProcessPatcher();

  // DeltaUpdateOpPatch::OutOfProcessPatcher implementation.
  virtual void Patch(const std::string& operation,
                     scoped_refptr<base::SequencedTaskRunner> task_runner,
                     base::FilePath& input_abs_path,
                     base::FilePath& patch_abs_path,
                     base::FilePath& output_abs_path,
                     base::Callback<void(int result)> callback) OVERRIDE;

 private:
  virtual ~ChromeOutOfProcessPatcher();

  scoped_refptr<PatchHost> host_;

  DISALLOW_COPY_AND_ASSIGN(ChromeOutOfProcessPatcher);
};

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_PATCHER_OPERATION_OUT_OF_PROCESS_H_
