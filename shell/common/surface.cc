// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/common/surface.h"
#include "lib/fxl/logging.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace shell {

SurfaceFrame::SurfaceFrame(sk_sp<SkSurface> surface,
                           SubmitCallback submit_callback)
    : submitted_(false), surface_(surface), submit_callback_(submit_callback) {
  FXL_DCHECK(submit_callback_);
}

SurfaceFrame::~SurfaceFrame() {
  if (submit_callback_) {
    // Dropping without a Submit.
    submit_callback_(*this, nullptr);
  }
}

bool SurfaceFrame::Submit() {
  if (submitted_) {
    return false;
  }

  submitted_ = PerformSubmit();

  return submitted_;
}

SkCanvas* SurfaceFrame::SkiaCanvas() {
  return surface_ != nullptr ? surface_->getCanvas() : nullptr;
}

sk_sp<SkSurface> SurfaceFrame::SkiaSurface() const {
  return surface_;
}

bool SurfaceFrame::PerformSubmit() {
  if (submit_callback_ == nullptr) {
    return false;
  }

  if (submit_callback_(*this, SkiaCanvas())) {
    return true;
  }

  return false;
}

Surface::Surface() : scale_(1.0) {}

Surface::~Surface() = default;

bool Surface::SupportsScaling() const {
  return false;
}

double Surface::GetScale() const {
  return scale_;
}

void Surface::SetScale(double scale) {
  static constexpr double kMaxScale = 1.0;
  static constexpr double kMinScale = 0.25;
  if (scale > kMaxScale) {
    scale = kMaxScale;
  } else if (scale < kMinScale) {
    scale = kMinScale;
  }
  scale_ = scale;
}

}  // namespace shell
