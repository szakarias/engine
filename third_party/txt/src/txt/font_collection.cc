/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "font_collection.h"

#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "font_skia.h"
#include "lib/fxl/logging.h"
#include "txt/platform.h"
#include "txt/text_style.h"

namespace txt {

namespace {

// Example characters representing character classes that may require a
// fallback font.
const std::vector<SkUnichar> fallback_characters{
    0x1f600,  // emoji
    0x4e00,   // CJK
    0x5d0,    // Hebrew
    0x627,    // Arabic
};

}  // anonymous namespace

FontCollection::FontCollection() = default;

FontCollection::~FontCollection() = default;

size_t FontCollection::GetFontManagersCount() const {
  return skia_font_managers_.size();
}

void FontCollection::PushFront(sk_sp<SkFontMgr> skia_font_manager) {
  if (!skia_font_manager) {
    return;
  }
  UpdateFallbackFonts(skia_font_manager);
  skia_font_managers_.push_front(std::move(skia_font_manager));
}

void FontCollection::PushBack(sk_sp<SkFontMgr> skia_font_manager) {
  if (!skia_font_manager) {
    return;
  }
  UpdateFallbackFonts(skia_font_manager);
  skia_font_managers_.push_back(std::move(skia_font_manager));
}

std::shared_ptr<minikin::FontCollection>
FontCollection::GetMinikinFontCollectionForFamily(const std::string& family) {
  // Look inside the font collections cache first.
  auto cached = font_collections_cache_.find(family);
  if (cached != font_collections_cache_.end()) {
    return cached->second;
  }

  for (sk_sp<SkFontMgr> manager : skia_font_managers_) {
    auto font_style_set = manager->matchFamily(family.c_str());
    if (font_style_set == nullptr || font_style_set->count() == 0) {
      continue;
    }

    std::vector<minikin::Font> minikin_fonts;

    // Add fonts to the Minikin font family.
    for (int i = 0, style_count = font_style_set->count(); i < style_count;
         ++i) {
      // Create the skia typeface.
      auto skia_typeface =
          sk_ref_sp<SkTypeface>(font_style_set->createTypeface(i));
      if (skia_typeface == nullptr) {
        continue;
      }

      // Create the minikin font from the skia typeface.
      // Divide by 100 because the weights are given as "100", "200", etc.
      minikin::Font minikin_font(
          std::make_shared<FontSkia>(skia_typeface),
          minikin::FontStyle{skia_typeface->fontStyle().weight() / 100,
                             skia_typeface->isItalic()});

      minikin_fonts.emplace_back(std::move(minikin_font));
    }

    // Create a Minikin font family.
    auto minikin_family =
        std::make_shared<minikin::FontFamily>(std::move(minikin_fonts));

    // Create a vector of font families for the Minikin font collection.
    std::vector<std::shared_ptr<minikin::FontFamily>> minikin_families = {
        minikin_family,
    };
    for (const auto& fallback : fallback_fonts_)
      minikin_families.push_back(fallback.second);

    // Create the minikin font collection.
    auto font_collection =
        std::make_shared<minikin::FontCollection>(std::move(minikin_families));

    // Cache the font collection for future queries.
    font_collections_cache_[family] = font_collection;

    return font_collection;
  }

  const auto default_font_family = GetDefaultFontFamily();
  if (family != default_font_family) {
    return GetMinikinFontCollectionForFamily(default_font_family);
  }

  // No match found in any of our font managers.
  return nullptr;
}

void FontCollection::UpdateFallbackFonts(sk_sp<SkFontMgr> manager) {
  char language_tag[ULOC_FULLNAME_CAPACITY];
  UErrorCode uerr;
  uloc_toLanguageTag(icu::Locale::getDefault().getName(), language_tag,
                     ULOC_FULLNAME_CAPACITY, FALSE, &uerr);
  if (U_FAILURE(uerr))
    return;
  const char* bcp47[] = {language_tag};

  for (SkUnichar fallback_char : fallback_characters) {
    if (fallback_fonts_.count(fallback_char))
      continue;

    sk_sp<SkTypeface> skia_typeface(manager->matchFamilyStyleCharacter(
        0, SkFontStyle(), bcp47, 1, fallback_char));
    if (!skia_typeface)
      continue;

    std::vector<minikin::Font> minikin_fonts;
    minikin_fonts.emplace_back(std::make_shared<FontSkia>(skia_typeface),
                               minikin::FontStyle());
    fallback_fonts_[fallback_char] =
        std::make_shared<minikin::FontFamily>(std::move(minikin_fonts));
  }
}

}  // namespace txt
