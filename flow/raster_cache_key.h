// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FLUTTER_FLOW_RASTER_CACHE_KEY_H_
#define FLUTTER_FLOW_RASTER_CACHE_KEY_H_

#include <unordered_map>
#include "flutter/flow/matrix_decomposition.h"
#include "lib/fxl/macros.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace flow {

class RasterCacheKey {
 public:
  RasterCacheKey(const SkPicture& picture, const MatrixDecomposition& matrix)
      : picture_id_(picture.uniqueID()),
        scale_key_(SkISize::Make(matrix.scale().x() * 1e3,
                                 matrix.scale().y() * 1e3)) {}

  uint32_t picture_id() const { return picture_id_; }

  const SkISize& scale_key() const { return scale_key_; }

  struct Hash {
    std::size_t operator()(RasterCacheKey const& key) const {
      return key.picture_id_;
    }
  };

  struct Equal {
    constexpr bool operator()(const RasterCacheKey& lhs,
                              const RasterCacheKey& rhs) const {
      return lhs.picture_id_ == rhs.picture_id_ &&
             lhs.scale_key_ == rhs.scale_key_;
    }
  };

  template <class Value>
  using Map = std::unordered_map<RasterCacheKey, Value, Hash, Equal>;

 private:
  uint32_t picture_id_;
  SkISize scale_key_;
};

}  // namespace flow

#endif  // FLUTTER_FLOW_RASTER_CACHE_KEY_H_
