// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/fml/message_loop.h"

#include <utility>

#include "flutter/fml/message_loop_impl.h"
#include "flutter/fml/task_runner.h"
#include "flutter/fml/thread_local.h"
#include "lib/fxl/memory/ref_counted.h"
#include "lib/fxl/memory/ref_ptr.h"

namespace fml {

FML_THREAD_LOCAL ThreadLocal tls_message_loop([](intptr_t value) {
  delete reinterpret_cast<MessageLoop*>(value);
});

MessageLoop& MessageLoop::GetCurrent() {
  auto loop = reinterpret_cast<MessageLoop*>(tls_message_loop.Get());
  FXL_CHECK(loop != nullptr)
      << "MessageLoop::EnsureInitializedForCurrentThread was not called on "
         "this thread prior to message loop use.";
  return *loop;
}

void MessageLoop::EnsureInitializedForCurrentThread() {
  if (tls_message_loop.Get() != 0) {
    // Already initialized.
    return;
  }
  tls_message_loop.Set(reinterpret_cast<intptr_t>(new MessageLoop()));
}

bool MessageLoop::IsInitializedForCurrentThread() {
  return tls_message_loop.Get() != 0;
}

MessageLoop::MessageLoop()
    : loop_(MessageLoopImpl::Create()),
      task_runner_(fxl::MakeRefCounted<fml::TaskRunner>(loop_)) {
  FXL_CHECK(loop_);
  FXL_CHECK(task_runner_);
}

MessageLoop::~MessageLoop() = default;

void MessageLoop::Run() {
  loop_->DoRun();
}

void MessageLoop::Terminate() {
  loop_->DoTerminate();
}

fxl::RefPtr<fxl::TaskRunner> MessageLoop::GetTaskRunner() const {
  return task_runner_;
}

fxl::RefPtr<MessageLoopImpl> MessageLoop::GetLoopImpl() const {
  return loop_;
}

void MessageLoop::AddTaskObserver(TaskObserver* observer) {
  loop_->AddTaskObserver(observer);
}

void MessageLoop::RemoveTaskObserver(TaskObserver* observer) {
  loop_->RemoveTaskObserver(observer);
}

}  // namespace fml
