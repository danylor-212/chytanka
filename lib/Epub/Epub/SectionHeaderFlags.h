#pragma once

#include <cstdint>

#include "EpubRenderMode.h"

// Text-transform flags share the section header's render-mode byte, above the
// EpubRenderMode values, so the header layout (and SECTION_FILE_VERSION) stays
// CrossInk's. A section built with Ukrainian typography or with a detected
// book language carries a byte that no build without those transforms writes,
// so a cache is rebuilt rather than reused when a device switches between such
// builds (Chytanka <-> stock CrossInk), in either direction. Sections without
// either transform are byte-identical to CrossInk's and stay shareable.
namespace section_header {

constexpr uint8_t RENDER_MODE_MASK = 0x3F;
constexpr uint8_t UKRAINIAN_TYPOGRAPHY = 0x80;
constexpr uint8_t DETECTED_UKRAINIAN = 0x40;
static_assert(EPUB_RENDER_MODE_COUNT <= RENDER_MODE_MASK + 1, "render modes collide with section flags");

constexpr uint8_t renderModeByte(const EpubRenderMode mode, const bool ukrainianTypographyApplied,
                                 const bool ukrainianDetectedNotDeclared) {
  return static_cast<uint8_t>((static_cast<uint8_t>(mode) & RENDER_MODE_MASK) |
                              (ukrainianTypographyApplied ? UKRAINIAN_TYPOGRAPHY : 0) |
                              (ukrainianDetectedNotDeclared ? DETECTED_UKRAINIAN : 0));
}

}  // namespace section_header
