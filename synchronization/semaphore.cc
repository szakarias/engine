// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/synchronization/semaphore.h"
#include "lib/fxl/build_config.h"
#include "lib/fxl/logging.h"

#if OS_MACOSX

#include <dispatch/dispatch.h>

namespace flutter {

class PlatformSemaphore {
 public:
  explicit PlatformSemaphore(uint32_t count)
      : _sem(dispatch_semaphore_create(count)) {}

  ~PlatformSemaphore() {
    if (_sem != nullptr) {
      dispatch_release(reinterpret_cast<dispatch_object_t>(_sem));
      _sem = nullptr;
    }
  }

  bool IsValid() const { return _sem != nullptr; }

  bool TryWait() {
    if (_sem == nullptr) {
      return false;
    }

    return dispatch_semaphore_wait(_sem, DISPATCH_TIME_NOW) == 0;
  }

  void Signal() {
    if (_sem != nullptr) {
      dispatch_semaphore_signal(_sem);
    }
  }

 private:
  dispatch_semaphore_t _sem;

  FXL_DISALLOW_COPY_AND_ASSIGN(PlatformSemaphore);
};

}  // namespace flutter

#else  // OS_MACOSX

#include <semaphore.h>
#include "lib/fxl/files/eintr_wrapper.h"

namespace flutter {

class PlatformSemaphore {
 public:
  explicit PlatformSemaphore(uint32_t count)
      : valid_(::sem_init(&sem_, 0 /* not shared */, count) == 0) {}

  ~PlatformSemaphore() {
    if (valid_) {
      int result = ::sem_destroy(&sem_);
      // Can only be EINVAL which should not be possible since we checked for
      // validity.
      FXL_DCHECK(result == 0);
    }
  }

  bool IsValid() const { return valid_; }

  bool TryWait() {
    if (!valid_) {
      return false;
    }

    return HANDLE_EINTR(::sem_trywait(&sem_)) == 0;
  }

  void Signal() {
    if (!valid_) {
      return;
    }

    ::sem_post(&sem_);

    return;
  }

 private:
  bool valid_;
  sem_t sem_;

  FXL_DISALLOW_COPY_AND_ASSIGN(PlatformSemaphore);
};

}  // namespace flutter

#endif

namespace flutter {

Semaphore::Semaphore(uint32_t count) : _impl(new PlatformSemaphore(count)) {}

Semaphore::~Semaphore() = default;

bool Semaphore::IsValid() const {
  return _impl->IsValid();
}

bool Semaphore::TryWait() {
  return _impl->TryWait();
}

void Semaphore::Signal() {
  return _impl->Signal();
}

}  // namespace flutter
