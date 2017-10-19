// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/lib/ui/painting/image_shader.h"

#include "lib/tonic/converter/dart_converter.h"
#include "lib/tonic/dart_args.h"
#include "lib/tonic/dart_binding_macros.h"
#include "lib/tonic/dart_library_natives.h"

using tonic::ToDart;

namespace blink {

static void ImageShader_constructor(Dart_NativeArguments args) {
  DartCallConstructor(&ImageShader::Create, args);
}

IMPLEMENT_WRAPPERTYPEINFO(ui, ImageShader);

#define FOR_EACH_BINDING(V) V(ImageShader, initWithImage)

FOR_EACH_BINDING(DART_NATIVE_CALLBACK)

void ImageShader::RegisterNatives(tonic::DartLibraryNatives* natives) {
  natives->Register(
      {{"ImageShader_constructor", ImageShader_constructor, 1, true},
       FOR_EACH_BINDING(DART_REGISTER_NATIVE)});
}

fxl::RefPtr<ImageShader> ImageShader::Create() {
  return fxl::MakeRefCounted<ImageShader>();
}

void ImageShader::initWithImage(CanvasImage* image,
                                SkShader::TileMode tmx,
                                SkShader::TileMode tmy,
                                const tonic::Float64List& matrix4) {
  if (!image)
    Dart_ThrowException(
        ToDart("ImageShader constructor called with non-genuine Image."));
  SkMatrix sk_matrix = ToSkMatrix(matrix4);
  set_shader(image->image()->makeShader(tmx, tmy, &sk_matrix));
}

ImageShader::ImageShader() : Shader(nullptr) {}

ImageShader::~ImageShader() {}

}  // namespace blink
