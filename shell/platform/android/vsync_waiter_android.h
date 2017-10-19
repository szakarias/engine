// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHELL_PLATFORM_ANDROID_VSYNC_WAITER_ANDROID_H_
#define SHELL_PLATFORM_ANDROID_VSYNC_WAITER_ANDROID_H_

#include <jni.h>
#include "flutter/shell/common/vsync_waiter.h"
#include "lib/fxl/macros.h"
#include "lib/fxl/memory/weak_ptr.h"

namespace shell {

class VsyncWaiterAndroid : public VsyncWaiter {
 public:
  VsyncWaiterAndroid();

  ~VsyncWaiterAndroid() override;

  static bool Register(JNIEnv* env);

  void AsyncWaitForVsync(Callback callback) override;

  void OnVsync(int64_t frameTimeNanos, int64_t frameTargetTimeNanos);

 private:
  Callback callback_;
  fxl::WeakPtr<VsyncWaiterAndroid> self_;

  fxl::WeakPtrFactory<VsyncWaiterAndroid> weak_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(VsyncWaiterAndroid);
};

}  // namespace shell

#endif  // SHELL_PLATFORM_ANDROID_ASYNC_WAITER_ANDROID_H_
