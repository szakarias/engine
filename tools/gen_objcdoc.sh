#!/usr/bin/env bash
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Generates objc docs for Flutter iOS libraries.

jazzy \
  --objc\
  --clean\
  --author Flutter Team\
  --author_url 'https://flutter.io'\
  --github_url 'https://github.com/flutter'\
  --github-file-prefix 'http://github.com/flutter/engine/blob/master'\
  --module-version 1.0.0\
  --xcodebuild-arguments --objc,flutter/shell/platform/darwin/ios/framework/Headers/Flutter.h,--,-x,objective-c,-isysroot,$(xcrun --show-sdk-path),-I,$(pwd)\
  --module Flutter\
  --root-url https://docs.flutter.io/objc/\
  --output $1\
  --readme './flutter/README.md'\
  --no-download-badge
