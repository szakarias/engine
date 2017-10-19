// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FLOW_LAYERS_PHYSICAL_MODEL_LAYER_H_
#define FLUTTER_FLOW_LAYERS_PHYSICAL_MODEL_LAYER_H_

#include "flutter/flow/layers/container_layer.h"

namespace flow {

class PhysicalModelLayer : public ContainerLayer {
 public:
  PhysicalModelLayer();
  ~PhysicalModelLayer() override;

  void set_rrect(const SkRRect& rrect) { rrect_ = rrect; }
  void set_elevation(float elevation) { elevation_ = elevation; }
  void set_color(SkColor color) { color_ = color; }
  void set_device_pixel_ratio(SkScalar dpr) { device_pixel_ratio_ = dpr; }

  static void DrawShadow(SkCanvas* canvas,
                         const SkPath& path,
                         SkColor color,
                         float elevation,
                         bool transparentOccluder,
                         SkScalar dpr);

  void Preroll(PrerollContext* context, const SkMatrix& matrix) override;

  void Paint(PaintContext& context) const override;

#if defined(OS_FUCHSIA)
  void UpdateScene(SceneUpdateContext& context) override;
#endif  // defined(OS_FUCHSIA)

 private:
  SkRRect rrect_;
  float elevation_;
  SkColor color_;
  SkScalar device_pixel_ratio_;
};

}  // namespace flow

#endif  // FLUTTER_FLOW_LAYERS_PHYSICAL_MODEL_LAYER_H_
