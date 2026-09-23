#pragma once

#include <I18n.h>
#include <SdCardFontRegistry.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

struct FontFamilyPointSizeRange {
  uint8_t first = 0;
  uint8_t last = 0;

  bool isValid() const { return first != 0; }
};

inline FontFamilyPointSizeRange fontFamilyPointSizeRange(const SdCardFontFamilyInfo& family) {
  if (family.firstSize) return {family.firstSize, family.lastSize};
  FontFamilyPointSizeRange range;
  for (const auto& file : family.files) {
    if (file.style != 0) continue;
    if (!range.isValid() || file.pointSize < range.first) range.first = file.pointSize;
    if (file.pointSize > range.last) range.last = file.pointSize;
  }
  return range;
}

inline std::string fontFamilyLabel(const std::string_view familyName, const FontFamilyPointSizeRange range) {
  std::string label;
  label.reserve(familyName.size() + 24);
  label.append(familyName.data(), familyName.size());
  if (!range.isValid()) return label;

  char sizes[32];
  if (range.last != range.first) {
    snprintf(sizes, sizeof(sizes), tr(STR_POINT_SIZE_RANGE_FMT), static_cast<unsigned>(range.first),
             static_cast<unsigned>(range.last));
  } else {
    snprintf(sizes, sizeof(sizes), tr(STR_POINT_SIZE_COMPACT_FMT), static_cast<unsigned>(range.first));
  }
  label += " (";
  label += sizes;
  label += ")";
  return label;
}
